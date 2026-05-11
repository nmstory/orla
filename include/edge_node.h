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
		uint16_t port = 4001; // init to default
		if (char *e = std::getenv("PORT")) port = std::atoi(e);
		adapter.start("0.0.0.0", port);

		std::cout << "[Edge] Started on port " << port << std::endl;

		Peer controller = adapter.setupPeer("controller", 4000);

		adapter.on_receive([this](const Message &msg, const std::string &ip, const uint16_t port) {
			std::cout << "[Edge] Received from " << ip << ":" << port
					  << " | Type: " << static_cast<int>(msg.type())
					  << " | Payload: " << msg.payload() << std::endl;


			if (msg.type() == MessageType::ClientAssigned) {
				Peer client = adapter.setupPeer(ip, port);
				clients.push_back(client);
			}
		});

		std::cout << "[Edge] Started on port 4001, connected to controller" << std::endl;

		int seq = 0;
		while (true) {
			Message msg = Message::Builder()
							  .type(MessageType::Heartbeat)
							  .payload(std::to_string(clients.size()))
							  .sequence(seq++)
							  .build();

			adapter.enqueue(msg, controller);
			std::cout << "[Edge] Sent heartbeat that the number of connected clients are " << clients.size() << std::endl;


			std::this_thread::sleep_for(std::chrono::seconds(2));
			// todo: edge logic
		}
	}
private:
	JuntosAdapter adapter;
	std::vector<Peer> clients;
};
} // namespace orla
