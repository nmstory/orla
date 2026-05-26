#include <client.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <message.h>
#include <thread>

namespace orla {

void Client::run() {
	m_Adapter.initMetrics(m_Registry);

	uint16_t port = 4002; // init to default
	if (char *e = std::getenv("PORT")) port = std::atoi(e);
	m_Adapter.start("0.0.0.0", port);
	std::cout << "[Client] Started on port " << port << std::endl;

	Peer controller = m_Adapter.setupPeer("controller", 4000);

	m_Adapter.on_receive([](const Message &msg, const std::string &ip, const uint16_t port) {
		std::cout << "[Client] Response from " << ip << ":" << port
				  << " | Payload: " << msg.payload() << std::endl;
	});

	m_Adapter.on_receive([this](const Message &msg, const std::string &ip, const uint16_t port) {
		if (msg.type() == MessageType::ClientConnectReqPong) {
			// response from controller to client to matchmake
			// parse "172.18.0.3:4001"
			std::string payload = msg.payload();
			auto colon = payload.find(':');
			std::string edge_ip = payload.substr(0, colon);
			uint16_t edge_port = std::stoi(payload.substr(colon + 1));

			m_WorkerEdge = new Peer(m_Adapter.setupPeer(edge_ip, edge_port));
		}
		else if (msg.type() == MessageType::ClientWorkResult) {
			WorkResult result;
			std::memcpy(&result, msg.payload().data(), sizeof(result));
			std::cout << "[Client] Work complete task_id=" << result.task_id << std::endl;
		}
	});

	int seq = 100;
	while (true) {
		if (m_WorkerEdge) {
			sendWorkRequest(seq++);
		} else {
			Message msg = Message::Builder()
				.type(MessageType::ClientConnectReqPing)
				.sequence(seq++)
				.build();
			m_Adapter.enqueue(msg, controller);
			std::cout << "[Client] Sent ping to controller" << std::endl;
		}
		std::this_thread::sleep_for(std::chrono::seconds(5));
	}
}

void Client::sendWorkRequest(int seq) {
	WorkRequest req{m_NextTaskId++, m_TaskDurationDistribution(m_Rng)};
	std::string payload(sizeof(req), '\0');
	std::memcpy(payload.data(), &req, sizeof(req));

	Message msg = Message::Builder()
		.type(MessageType::ClientWorkRequest)
		.payload(std::move(payload))
		.sequence(seq)
		.build();
	m_Adapter.enqueue(msg, *m_WorkerEdge);
	std::cout << "[Client] Sent work request task_id=" << req.task_id
			  << " duration_ms=" << req.duration_ms << std::endl;
}

} // namespace orla
