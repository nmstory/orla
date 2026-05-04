#pragma once
#include "node_interface.h"
#include <config.h>
#include <chrono>
#include <iostream>
#include <message.h>
#include <network_adapter.h>
#include <thread>
#include <vector>

namespace orla {

struct KnownPeer {
	KnownPeer(Peer peer) : m_Peer(peer), m_TimeOfLastAck(std::chrono::system_clock::now()) {}
	KnownPeer(const KnownPeer &) = default;
	KnownPeer& operator=(KnownPeer&& other) noexcept {
		m_Peer.sendAddr = other.m_Peer.sendAddr;
		m_TimeOfLastAck = std::move(other.m_TimeOfLastAck);
		return *this;
	}

	std::chrono::duration<double> timeSinceLastAck() const {
		return std::chrono::system_clock::now() - m_TimeOfLastAck;
	}

	void refreshAck() { m_TimeOfLastAck = std::chrono::system_clock::now(); }

	Peer m_Peer;
	std::chrono::system_clock::time_point m_TimeOfLastAck;
};

class Controller : public NodeInterface {
  public:
	void run() override {
		adapter.on_receive([this](const Message &msg, const std::string &ip, uint16_t port) {
			std::cout << "[Controller] Received from " << ip << ":" << port
					  << " | Type: " << static_cast<int>(msg.type())
					  << " | Payload: " << msg.payload() << std::endl;

			if (msg.type() == MessageType::Heartbeat) {
				auto it = findEdge(ip, port);
				if (it != alive_edges.end())
					it->refreshAck();
				else
					alive_edges.push_back(adapter.setupPeer(ip, port));
				removeDeadEdges();
			} else if (msg.type() == MessageType::ClientConnectReqPing) {
				if (!alive_edges.empty()) {
					KnownPeer &edge = alive_edges[0]; // TODO: pick by load
					char ip_buf[INET_ADDRSTRLEN];
					inet_ntop(AF_INET, &edge.m_Peer.sendAddr.sin_addr, ip_buf, INET_ADDRSTRLEN);
					uint16_t edge_port = ntohs(edge.m_Peer.sendAddr.sin_port);

					Message response = Message::Builder()
						.type(MessageType::ClientConnectReqPong)
						.payload(std::string(ip_buf) + ":" + std::to_string(edge_port))
						.sequence(msg.sequence())
						.build();
					adapter.enqueue(response, adapter.setupPeer(ip, port));
				}
			}
		});

		uint16_t port = 4000;
		if (char *e = std::getenv("PORT")) port = std::atoi(e);
		adapter.start("0.0.0.0", port);
		std::cout << "[Controller] Listening on port " << port << std::endl;

		while (true)
			std::this_thread::sleep_for(std::chrono::seconds(1));
	}

  private:
	std::vector<KnownPeer>::iterator findEdge(const std::string &ip, uint16_t port) {
		return std::find_if(alive_edges.begin(), alive_edges.end(), [&](const KnownPeer &p) {
			return p.m_Peer.sendAddr.sin_port == htons(port) &&
				   p.m_Peer.sendAddr.sin_addr.s_addr == inet_addr(ip.c_str());
		});
	}

	void removeDeadEdges() {
		std::erase_if(alive_edges, [](const KnownPeer &p) {
			return p.timeSinceLastAck() > std::chrono::seconds(orla::config::HEARTBEAT_TIMEOUT_S);
		});
	}

	JuntosAdapter adapter{};
	std::vector<KnownPeer> alive_edges{};
};

} // namespace orla
