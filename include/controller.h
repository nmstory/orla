#pragma once
#include "node_interface.h"
#include <chrono>
#include <iostream>
#include <message.h>
#include <network_adapter.h>
#include <thread>

#include <client.h>
#include <edge_node.h>
#include <vector>

namespace orla {

class Controller : public NodeInterface {
  public:
	Controller() {
	}
	~Contoller() {
	}
	void run() override {
		adapter.on_receive([](const Message &msg, const std::string &ip, const uint16_t port) {
			std::cout << "[Controller] Received from " << ip << ":" << port
					  << " | Type: " << static_cast<int>(msg.type())
					  << " | Payload: " << msg.payload() << std::endl;
		});

		adapter.on_receive([this](const Message &msg, const std::string &ip, const uint16_t port) {
			if (msg.type() == MessageType::Heartbeat) {
				if (isEdgeKnown(ip, port)) {
					// edge is known, just cout
					// todo: ACK heartbeat and remove/re-org clients for disconnected edges
				} else {
					// edge isn't known yet
					Peer edge = adapter.setupPeer(ip, port);
					alive_edges.push_back(edge);
				}
			}
			if (msg.type() == MessageType::ClientConnectReqPing) {
				Peer client = adapter.setupPeer(ip, port);
				alive_clients.push_back(client);

				Message response = Message::Builder()
									   .type(MessageType::ClientConnectReqPong)
									   .payload("Pong") // START HERE - want to respond with the edge node the client has been allocated. This will be based on Grafana data soon.
									   .sequence(msg.sequence())
									   .build();
				adapter.enqueue(response, client);
				std::cout << "[Controller] Sent Pong to " << ip << ":" << port << std::endl;
			}
		});

		adapter.start("0.0.0.0", 4000);
		std::cout << "[Controller] Listening on port 4000..." << std::endl;

		while (true) {
			std::this_thread::sleep_for(std::chrono::seconds(1));
			// todo: controller logic
		}
	}

	// todo: how do we compare a peer to an ip and port? Easier to modify Peer class or compare sockaddr_in?
	bool isEdgeKnown(const std::string &ip, const uint16_t port) const {
		for (Peer edge : alive_edges) {
			if (edge.ip == ip &&)
		}
		return false;
	}

  private:
	JuntosAdapter adapter{};
	std::vector<Peer> alive_edges{};
	std::vector<Peer> alive_clients{};
};

} // namespace orla
