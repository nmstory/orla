#pragma once
#include "node_interface.h"
#include <chrono>
#include <iostream>
#include <message.h>
#include <network_adapter.h>
#include <thread>
#include <vector>

namespace orla {

class EdgeNode : public NodeInterface {
public:
	explicit EdgeNode(prometheus::Registry& registry) : NodeInterface(registry) {}

	void run() override {
		uint16_t port = 4001;
		if (char *e = std::getenv("PORT")) port = std::atoi(e);
		m_Adapter.start("0.0.0.0", port);
		std::cout << "[Edge] Started on port " << port << std::endl;

		Peer controller = m_Adapter.setupPeer("controller", 4000);

		m_Adapter.on_receive([this, &controller](const Message &msg, const std::string &ip, uint16_t port) {
			std::cout << "[Edge] Received from " << ip << ":" << port
					  << " | Type: " << static_cast<int>(msg.type())
					  << " | Payload: " << msg.payload() << std::endl;

			if (msg.type() == MessageType::Heartbeat) {
				// ping from controller — respond with client count
				Message ack = Message::Builder()
					.type(MessageType::HeartbeatAck)
					.payload(std::to_string(m_Clients.size()))
					.build();
				m_Adapter.enqueue(ack, controller);
			}
			else if (msg.type() == MessageType::ClientAssigned) {
				m_Clients.push_back(m_Adapter.setupPeer(ip, port));
			}
			else if (msg.type() == MessageType::ClientWorkRequest) {
				Peer client = m_Adapter.setupPeer(ip, port);
				processWorkRequest(msg, client);
			}
		});

		std::cout << "[Edge] Connected to controller" << std::endl;

		int seq = 0;
		while (true) {
			// periodic heartbeat for registration/keepalive
			Message msg = Message::Builder()
				.type(MessageType::Heartbeat)
				.payload(std::to_string(m_Clients.size()))
				.sequence(seq++)
				.build();
			m_Adapter.enqueue(msg, controller);
			std::cout << "[Edge] Heartbeat — clients: " << m_Clients.size() << std::endl;

			std::this_thread::sleep_for(std::chrono::seconds(2));
		}
	}

private:
	void processWorkRequest(const Message &msg, Peer client) {
		WorkRequest req = *reinterpret_cast<const WorkRequest *>(msg.payload().data());
		std::cout << "[Edge] Processing task_id=" << req.task_id
				  << " duration_ms=" << req.duration_ms << std::endl;

		std::thread([this, req, client]() {
			std::this_thread::sleep_for(std::chrono::milliseconds(req.duration_ms));

			WorkResult result{req.task_id};
			std::string payload(sizeof(result), '\0');
			std::memcpy(payload.data(), &result, sizeof(result));

			Message ack = Message::Builder()
				.type(MessageType::ClientWorkResult)
				.payload(std::move(payload))
				.build();
			m_Adapter.enqueue(ack, client);
			std::cout << "[Edge] Completed task_id=" << req.task_id << std::endl;
		}).detach();
	}

	JuntosAdapter m_Adapter;
	std::vector<Peer> m_Clients;
};

} // namespace orla
