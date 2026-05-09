#pragma once
#include "node_interface.h"
#include <chrono>
#include <iostream>
#include <message.h>
#include <network_adapter.h>
#include <thread>

namespace orla {

class Client : public NodeInterface {
public:
	void run() override {
		uint16_t port = 4002; // init to default
		if (char *e = std::getenv("PORT")) port = std::atoi(e);
		adapter.start("0.0.0.0", port);
		std::cout << "[Client] Started on port " << port << std::endl;

		Peer controller = adapter.setupPeer("controller", 4000);

		adapter.on_receive([](const Message &msg, const std::string &ip, const uint16_t port) {
			std::cout << "[Client] Response from " << ip << ":" << port
					  << " | Payload: " << msg.payload() << std::endl;
		});

		adapter.on_receive([this](const Message &msg, const std::string &ip, const uint16_t port) {
			if (msg.type() == MessageType::ClientConnectReqPong) {
				// response from controller to client to matchmake
				// parse "172.18.0.3:4001"
				std::string payload = msg.payload();
				auto colon = payload.find(':');
				std::string edge_ip = payload.substr(0, colon);
				uint16_t edge_port = std::stoi(payload.substr(colon + 1));

				workerEdge = new Peer(adapter.setupPeer(edge_ip, edge_port));

			}
		});

		int seq = 100;
		while (true) {
			if (workerEdge) {
				Message msg = Message::Builder()
					.type(MessageType::Request)
					.payload("work")
					.sequence(seq++)
					.build();
				adapter.enqueue(msg, *workerEdge);  
				std::cout << "[Client] Sent request to edge" << std::endl;
			} else {
				Message msg = Message::Builder()
					.type(MessageType::ClientConnectReqPing)
					.sequence(seq++)
					.build();
				adapter.enqueue(msg, controller);
				std::cout << "[Client] Sent ping to controller" << std::endl;
			}
			std::this_thread::sleep_for(std::chrono::seconds(5));
		}

	}

	// todo: better name!
	//Peer parsePongCreatePeer(std::string s) const {
		// todo: take the payload and understand the ip and port, then create peer
	//}
private:
	JuntosAdapter adapter;
	Peer* workerEdge;
};

} // namespace orla
