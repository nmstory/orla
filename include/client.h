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
		Peer controller = adapter.addPeer("controller", 4000);

		adapter.on_receive([](const Message &msg, const std::string &ip, uint16_t port) {
			std::cout << "[Client] Response from " << ip << ":" << port
					  << " | Payload: " << msg.payload() << std::endl;
		});

		adapter.start("0.0.0.0", 4002);

		int seq = 100;
		while (true) {
			Message msg = Message::Builder()
							  .type(MessageType::Request)
							  .payload("Do work")
							  .sequence(seq++)
							  .build();
			adapter.send(msg, controller);
			std::cout << "[Client] Sent request to controller" << std::endl;
			std::this_thread::sleep_for(std::chrono::seconds(5));
		}
	}
};

} // namespace orla
