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
		JuntosAdapter adapter;

		adapter.start("0.0.0.0", 4002);
		std::cout << "[Client] Started on port 4002" << std::endl;

		Peer controller = adapter.setupPeer("controller", 4000);

		adapter.on_receive([](const Message &msg, const std::string &ip, const uint16_t port) {
			std::cout << "[Client] Response from " << ip << ":" << port
					  << " | Payload: " << msg.payload() << std::endl;
		});

		adapter.on_receive([](const Message &msg, const std::string &ip, const uint16_t port) {
			if (msg.type() == MessageType::ClientConnectReqPong) {
				// response from controller to client to matchmake
				// todo: read payload
			}
		});

		int seq = 100;
		Message msg = Message::Builder()
						  .type(MessageType::ClientConnectReqPing)
						  .sequence(seq++)
						  .build();
		adapter.enqueue(msg, controller);
		std::cout << "[Client] Sent connection request ping to controller" << std::endl;
		while (true) {
			Message msg = Message::Builder()
							  .type(MessageType::ClientConnectReqPing)
							  .sequence(seq++)
							  .build();
			adapter.enqueue(msg, controller);
			std::cout << "[Client] Sent request to controller" << std::endl;
			std::this_thread::sleep_for(std::chrono::seconds(5));
			// todo: complete client logic
		}
	}

	// todo: better name!
	Peer parsePongCreatePeer(std::string s) const {
		// todo: take the payload and understand the ip and port, then create peer
	}
};

} // namespace orla
