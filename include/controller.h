#pragma once
#include "node_interface.h"
#include <chrono>
#include <iostream>
#include <message.h>
#include <network_adapter.h>
#include <thread>

namespace orla {

class Controller : public NodeInterface {
  public:
	void run() override {
		JuntosAdapter adapter;
		adapter.on_receive([](const Message &msg, const std::string &ip, uint16_t port) {
			std::cout << "[Controller] Received from " << ip << ":" << port
					  << " | Type: " << static_cast<int>(msg.type())
					  << " | Payload: " << msg.payload() << std::endl;
		});

		adapter.start("0.0.0.0", 4000);
		std::cout << "[Controller] Listening on port 4000..." << std::endl;

		while (true) {
			std::this_thread::sleep_for(std::chrono::seconds(1));
			// todo: controller logic
		}
	}
};

} // namespace orla
