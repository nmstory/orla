#pragma once
#include "../external/juntos/include/network_handler.h"
#include "../external/juntos/include/session_linux.h"
#include "config.h"
#include "message.h"
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <prometheus/counter.h>
#include <prometheus/registry.h>
#include <queue>
#include <random>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <network_handler.h>
#include <sys/socket.h>

namespace orla {

class INetworkAdapter {
  public:
	virtual ~INetworkAdapter() = default;
	virtual void enqueue(const Message &msg, const Peer &peer) = 0;
	virtual void on_receive(
		std::function<void(const Message &, const std::string &, uint16_t)> callback) = 0;
	virtual void start(std::string hostname, uint16_t port) = 0;
};

class JuntosAdapter : public INetworkAdapter {
  public:
	JuntosAdapter() : m_Session(nullptr),
					  m_Rng(std::random_device{}()), m_Dist01(0.0, 1.0), m_Stopped(false) {}

	void enqueue(const Message &msg, const Peer &peer) override {
		auto buf = msg.serialize();
		std::vector<uint8_t> raw(buf.begin(), buf.end());

		if (m_Dist01(m_Rng) < config::LOSS_RATE) {
			if (m_PacketsDropped) m_PacketsDropped->Increment();
			return;
		}

		// add random latency
		int ms = config::MAX_LATENCY_MS > 0 ? std::uniform_int_distribution<int>(0, config::MAX_LATENCY_MS)(m_Rng) : 0;
		auto deliver_at = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);

		{ // push to pending queue
			std::lock_guard<std::mutex> lk(m_Mutex);
			m_Pending.push(Pending{deliver_at, peer.sendAddr, std::move(raw)});
			m_Cv.notify_one();
		}
	}

	void on_receive(
		std::function<void(const Message &, const std::string &, uint16_t)> callback) override {
		m_RecvCallback = std::move(callback);
	}

	void start(std::string hostname, uint16_t port) override {
		if (char *e = std::getenv("LOSS_RATE"))
			config::LOSS_RATE = std::stod(e);
		if (char *e = std::getenv("MAX_LATENCY_MS"))
			config::MAX_LATENCY_MS = std::atoi(e);
		if (char *e = std::getenv("REORDER_PROB"))
			config::REORDER_PROB = std::stod(e);

		if (!m_Session) {
			m_Session = new LinuxSession();
		}
		m_Session->initSessionSolo(hostname, port);

		auto receive_loop = [this]() {
			while (!m_Stopped) {
				auto [success, data, sender] = recvData(m_Session->getSocketFD());
				if (success && m_RecvCallback) {
					std::cout << "Received " << data.size() << std::endl;
					try{
						Message msg = Message::deserialize(reinterpret_cast<const uint8_t *>(data.data()), data.size());
						char ip[INET_ADDRSTRLEN];
						inet_ntop(AF_INET, &(sender.sin_addr), ip, INET_ADDRSTRLEN);
						uint16_t sender_port = ntohs(sender.sin_port);
						m_RecvCallback(msg, std::string(ip), sender_port);
						if (m_TotalBytesReceived) m_TotalBytesReceived->Increment(data.size());
					} catch (const std::exception &e){
						std::cerr << "Deserialise failed: " << e.what() << std::endl;
					}
				}
				std::this_thread::yield(); // TODO: cleanup blocking tech
			}
		};
		std::thread(receive_loop).detach();

		// sender worker
		m_Worker = std::thread([this] { this->workerLoop(); });
	}

	void initMetrics(prometheus::Registry& registry) {
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

	Peer setupPeer(const std::string &peer_addr, uint16_t port) {
		return m_Session->setupPeer(peer_addr, port);
	}

	~JuntosAdapter() {
		// stop worker
		{
			std::lock_guard<std::mutex> lk(m_Mutex);
			m_Stopped = true;
			m_Cv.notify_one();
		}
		if (m_Worker.joinable())
			m_Worker.join();
		delete m_Session;
	}

  private:
	std::function<void(const Message &, const std::string &, uint16_t)> m_RecvCallback;
	LinuxSession *m_Session;

	std::mt19937 m_Rng;
	std::uniform_real_distribution<double> m_Dist01;

	struct Pending {
		std::chrono::steady_clock::time_point when;
		sockaddr_in addr;
		std::vector<uint8_t> buf;
		bool operator>(Pending const &o) const { return when > o.when; }
	};

	std::priority_queue<Pending, std::vector<Pending>, std::greater<Pending>> m_Pending;
	std::mutex m_Mutex;
	std::condition_variable m_Cv;
	std::thread m_Worker;
	bool m_Stopped;

	prometheus::Counter* m_TotalBytesSent = nullptr;
	prometheus::Counter* m_TotalBytesReceived = nullptr;
	prometheus::Counter* m_PacketsDropped = nullptr;

	void workerLoop() {
		std::unique_lock<std::mutex> lk(m_Mutex);
		while (!m_Stopped) {
			if (m_Pending.empty()) {
				m_Cv.wait(lk, [this] { return m_Stopped || !m_Pending.empty(); });
				if (m_Stopped)
					break;
			}

			auto &pkt = m_Pending.top();
			auto now = std::chrono::steady_clock::now();
			if (pkt.when > now)
				m_Cv.wait_until(lk, pkt.when);
			else {
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
};

} // namespace orla
