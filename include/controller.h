#pragma once
#include "node_interface.h"
#include <algorithm>
#include <chrono>
#include <config.h>
#include <cstdlib>
#include <iostream>
#include <message.h>
#include <network_adapter.h>
#include <optional>
#include <thread>
#include <vector>

namespace orla {

struct Edge {
	Edge(Peer peer) : m_Peer(std::move(peer)) {}
	Edge(const Edge &) = default;
	Edge &operator=(Edge &&other) noexcept {
		m_Peer           = std::move(other.m_Peer);
		m_Score          = other.m_Score;
		m_LastPingSentAt = std::move(other.m_LastPingSentAt);
		m_LastAckAt      = std::move(other.m_LastAckAt);
		return *this;
	}

	Peer m_Peer;
	float m_Score = 0.0f;
	std::chrono::system_clock::time_point m_LastPingSentAt{std::chrono::system_clock::now()};
	std::chrono::system_clock::time_point m_LastAckAt{std::chrono::system_clock::now()};
};

class Controller : public NodeInterface {
  public:
	void run() override {
		adapter.on_receive([this](const Message &msg, const std::string &ip, uint16_t port) {
			if (msg.type() == MessageType::Heartbeat) {
				auto it = getEdge(ip, port);
				if (it == alive_edges.end())
					alive_edges.push_back(adapter.setupPeer(ip, port));
				removeDeadEdges();
			} else if (msg.type() == MessageType::HeartbeatAck) {
				auto it = getEdge(ip, port);
				if (it != alive_edges.end()) {
					int64_t latencyMs = std::chrono::duration_cast<std::chrono::milliseconds>(
						std::chrono::system_clock::now() - it->m_LastPingSentAt).count();
					uint8_t clientCount = std::stoi(msg.payload());
					it->m_Score     = config::LOAD_WEIGHT * clientCount + config::LATENCY_WEIGHT * latencyMs;
					it->m_LastAckAt = std::chrono::system_clock::now();
				}
			} else if (msg.type() == MessageType::ClientConnectReqPing) {
				auto it = getBestEdge();
				if (it != alive_edges.end() && it->m_Score >= config::SCALE_UP_THRESHOLD)
					spawnEdge();
				if (it != alive_edges.end()) {
					char ip_buf[INET_ADDRSTRLEN];
					inet_ntop(AF_INET, &it->m_Peer.sendAddr.sin_addr, ip_buf, INET_ADDRSTRLEN);
					uint16_t edge_port = ntohs(it->m_Peer.sendAddr.sin_port);

					Message response = Message::Builder()
						.type(MessageType::ClientConnectReqPong)
						.payload(std::string(ip_buf) + ":" + std::to_string(edge_port))
						.sequence(msg.sequence())
						.build();
					adapter.enqueue(response, adapter.setupPeer(ip, port));
				} else {
					std::cerr << "[Controller] No known edges to allocate client to." << std::endl;
				}
			}
		});

		uint16_t port = 4000;
		if (char *e = std::getenv("PORT")) port = std::atoi(e);
		adapter.start("0.0.0.0", port);
		std::cout << "[Controller] Listening on port " << port << std::endl;

		while (true) {
			std::this_thread::sleep_for(std::chrono::seconds(2));
			for (auto &edge : alive_edges) {
				edge.m_LastPingSentAt = std::chrono::system_clock::now();
				adapter.enqueue(Message::Builder().type(MessageType::Heartbeat).build(), edge.m_Peer);
			}
		}
	}

  private:
	std::vector<Edge>::iterator getEdge(const std::string &ip, uint16_t port) {
		return std::find_if(alive_edges.begin(), alive_edges.end(), [&](const Edge &p) {
			return p.m_Peer.sendAddr.sin_port == htons(port) &&
				   p.m_Peer.sendAddr.sin_addr.s_addr == inet_addr(ip.c_str());
		});
	}

	std::vector<Edge>::iterator getBestEdge() {
		return std::min_element(alive_edges.begin(), alive_edges.end(), [](const Edge &a, const Edge &b) {
			return a.m_Score < b.m_Score;
		});
	}

	void removeDeadEdges() {
		std::erase_if(alive_edges, [](const Edge &p) {
			return std::chrono::system_clock::now() - p.m_LastAckAt >
				   std::chrono::seconds(orla::config::HEARTBEAT_TIMEOUT_S);
		});
	}

	void spawnEdge() {
		std::string cmd = "docker run -d --network orla_orla_net "
						  "-e ROLE=edge -e PORT=" + std::to_string(m_NextEdgePort++) + " orla:latest";
		std::system(cmd.c_str());
		std::cout << "[Controller] Spawned new edge on port " << m_NextEdgePort - 1 << std::endl;
	}

	JuntosAdapter adapter{};
	std::vector<Edge> alive_edges{};
	uint16_t m_NextEdgePort = 4001;
};

} // namespace orla
