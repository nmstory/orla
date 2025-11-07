#pragma once
#include "node_interface.h"
#include <chrono>
#include <iostream>
#include <message.h>
#include <network_adapter.h>
#include <thread>

namespace orla {

class EdgeNode : public NodeInterface {
  public:
	void run() override {
		JuntosAdapter adapter;

		Peer controller = adapter.addPeer("controller", 4000);

		adapter.on_receive([](const Message &msg, const std::string &ip, uint16_t port) {
			std::cout << "[Edge] Received from " << ip << ":" << port
					  << " | Type: " << static_cast<int>(msg.type())
					  << " | Payload: " << msg.payload() << std::endl;
		});

		adapter.start("0.0.0.0", 4001);
		std::cout << "[Edge] Started on port 4001, connected to controller" << std::endl;

		int seq = 0;
		while (true) {
			Message msg = Message::Builder()
							  .type(MessageType::Heartbeat)
							  .payload("Edge alive")
							  .sequence(seq++)
							  .build();

			adapter.send(msg, controller);
			std::cout << "[Edge] Sent heartbeat" << std::endl;

			std::this_thread::sleep_for(std::chrono::seconds(2));
			// todo: edge logic
		}
	}
};

} // namespace orla
