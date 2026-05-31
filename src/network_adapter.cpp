#include <network_adapter.h>

#include <config.h>
#include <cstdlib>
#include <iostream>
#include <span>
#include <sys/time.h>

namespace orla {

void JuntosAdapter::enqueue(const Message &msg, const Peer &peer) {
	thread_local std::mt19937 rng{std::random_device{}()};
	thread_local std::uniform_real_distribution<double> dist01(0.0, 1.0);

	if (dist01(rng) < config::LOSS_RATE) {
		if (m_PacketsDropped) m_PacketsDropped->Increment();
		return;
	}

	int ms = config::MAX_LATENCY_MS > 0
		? std::uniform_int_distribution<int>(0, config::MAX_LATENCY_MS)(rng)
		: 0;
	auto deliver_at = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);

	auto buf = msg.serialize();
	{
		std::lock_guard<std::mutex> lk(m_Mutex);
		m_Pending.push(Pending{deliver_at, peer.sendAddr, std::move(buf)});
		m_Cv.notify_one();
	}
}

void JuntosAdapter::on_receive(
	std::function<void(const Message &, const std::string &, uint16_t)> callback) {
	m_RecvCallback = std::move(callback);
}

void JuntosAdapter::start(std::string hostname, uint16_t port) {
	if (char *e = std::getenv("LOSS_RATE"))
		config::LOSS_RATE = std::stod(e);
	if (char *e = std::getenv("MAX_LATENCY_MS"))
		config::MAX_LATENCY_MS = std::atoi(e);
	if (char *e = std::getenv("REORDER_PROB"))
		config::REORDER_PROB = std::stod(e);

	if (!m_Session) {
		m_Session = std::make_unique<LinuxSession>();
	}
	// 200ms recv timeout so the recv thread can poll its stop_token.
	m_Session->initSessionSolo(hostname, port, std::chrono::milliseconds(200));

	m_RecvThread = std::jthread([this](std::stop_token st) {
		while (!st.stop_requested()) {
			auto [success, data, sender] = recvData(m_Session->getSocketFD());
			if (!success || !m_RecvCallback) continue;

			std::cout << "Received " << data.size() << std::endl;
			try {
				Message msg = Message::deserialize(reinterpret_cast<const uint8_t *>(data.data()), data.size());
				char ip[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, &(sender.sin_addr), ip, INET_ADDRSTRLEN);
				uint16_t sender_port = ntohs(sender.sin_port);
				m_RecvCallback(msg, std::string(ip), sender_port);
				if (m_TotalBytesReceived) m_TotalBytesReceived->Increment(data.size());
			} catch (const std::exception &e) {
				std::cerr << "Deserialise failed: " << e.what() << std::endl;
			}
		}
	});

	m_Worker = std::jthread([this](std::stop_token st) { this->workerLoop(st); });
}

void JuntosAdapter::initMetrics(prometheus::Registry& registry) {
	m_TotalBytesSent = &prometheus::BuildCounter()
		.Name("bytes_sent_total")
		.Help("Total bytes sent")
		.Register(registry).Add({});

	m_TotalBytesReceived = &prometheus::BuildCounter()
		.Name("bytes_received_total")
		.Help("Total bytes received")
		.Register(registry).Add({});

	m_PacketsDropped = &prometheus::BuildCounter()
		.Name("bytes_dropped_total")
		.Help("Total packets dropped due to simulated loss")
		.Register(registry).Add({});
}

Peer JuntosAdapter::setupPeer(const std::string &peer_addr, uint16_t port) {
	return m_Session->setupPeer(peer_addr, port);
}

void JuntosAdapter::workerLoop(std::stop_token st) {
	std::unique_lock<std::mutex> lk(m_Mutex);
	while (!st.stop_requested()) {
		if (m_Pending.empty()) {
			m_Cv.wait(lk, st, [this] { return !m_Pending.empty(); });
			if (st.stop_requested()) break;
		}

		auto &pkt = m_Pending.top();
		auto now = std::chrono::steady_clock::now();
		if (pkt.when > now) {
			m_Cv.wait_until(lk, st, pkt.when, [] { return false; });
		} else {
			Pending send_pkt = std::move(const_cast<Pending &>(m_Pending.top()));
			m_Pending.pop();
			lk.unlock();
			if (m_Session) {
				int sock = m_Session->getSocketFD();
				std::span<const std::byte> payload(reinterpret_cast<const std::byte *>(send_pkt.buf.data()), send_pkt.buf.size());
				char dest_ip[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, &send_pkt.addr.sin_addr, dest_ip, INET_ADDRSTRLEN);
				std::cout << "[Worker] Sending " << payload.size() << " bytes to "
						<< dest_ip << ":" << ntohs(send_pkt.addr.sin_port)
						<< " on sock " << sock << std::endl;
				sendData<int>(sock, send_pkt.addr, payload, payload.size());
				if (m_TotalBytesSent) m_TotalBytesSent->Increment(send_pkt.buf.size());
			}
			lk.lock();
		}
	}
}

} // namespace orla
