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
	void run() override {
		uint16_t port = 4001;
		if (char *e = std::getenv("PORT")) port = std::atoi(e);
		adapter.start("0.0.0.0", port);
		std::cout << "[Edge] Started on port " << port << std::endl;

		Peer controller = adapter.setupPeer("controller", 4000);

		adapter.on_receive([this, &controller](const Message &msg, const std::string &ip, uint16_t port) {
			std::cout << "[Edge] Received from " << ip << ":" << port
					  << " | Type: " << static_cast<int>(msg.type())
					  << " | Payload: " << msg.payload() << std::endl;

			if (msg.type() == MessageType::Heartbeat) {
				// ping from controller — respond with client count
				Message ack = Message::Builder()
					.type(MessageType::HeartbeatAck)
					.payload(std::to_string(clients.size()))
					.build();
				adapter.enqueue(ack, controller);
			} 
			else if (msg.type() == MessageType::ClientAssigned) {
				clients.push_back(adapter.setupPeer(ip, port));
			}
			else if (msg.type() == MessageType::ClientWorkRequest) {
				std::cout << "[Edge] Received work to be done from the client" << std::endl;
			}
		});

		std::cout << "[Edge] Connected to controller" << std::endl;

		int seq = 0;
		while (true) {
			// periodic heartbeat for registration/keepalive
			Message msg = Message::Builder()
				.type(MessageType::Heartbeat)
				.payload(std::to_string(clients.size()))
				.sequence(seq++)
				.build();
			adapter.enqueue(msg, controller);
			std::cout << "[Edge] Heartbeat — clients: " << clients.size() << std::endl;

			std::this_thread::sleep_for(std::chrono::seconds(2));
		}
	}

private:
	JuntosAdapter adapter;
	std::vector<Peer> clients;
};

} // namespace orla
