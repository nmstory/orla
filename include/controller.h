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
#include <prometheus/counter.h>
#include <prometheus/gauge.h>
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
	explicit Controller(prometheus::Registry& registry) : NodeInterface(registry) {
		m_ActiveEdges = &prometheus::BuildGauge()
			.Name("active_edges")
			.Help("The number of edges currently active within this session")
			.Register(m_Registry).Add({});
		
		auto& scaleFamily = prometheus::BuildCounter()
			.Name("scaling_events_total")
			.Help("Autoscaling events")
			.Labels({{"direction", ""}})
			.Register(m_Registry);

		m_ScaleUp   = &scaleFamily.Add({{"direction", "up"}});
		m_ScaleDown = &scaleFamily.Add({{"direction", "down"}});
	}

	void run() override {
		m_Adapter.initMetrics(m_Registry);

		m_Adapter.on_receive([this](const Message &msg, const std::string &ip, uint16_t port) {
			if (msg.type() == MessageType::Heartbeat) {
				auto it = getEdge(ip, port);
				if (it == m_AliveEdges.end())
					m_AliveEdges.push_back(m_Adapter.setupPeer(ip, port));
				removeDeadEdges();
				m_ActiveEdges->Set(m_AliveEdges.size());
			} else if (msg.type() == MessageType::HeartbeatAck) {
				auto it = getEdge(ip, port);
				if (it != m_AliveEdges.end()) {
					int64_t latencyMs = std::chrono::duration_cast<std::chrono::milliseconds>(
						std::chrono::system_clock::now() - it->m_LastPingSentAt).count();
					uint8_t clientCount = std::stoi(msg.payload());
					it->m_Score     = config::LOAD_WEIGHT * clientCount + config::LATENCY_WEIGHT * latencyMs;
					it->m_LastAckAt = std::chrono::system_clock::now();
				}
			} else if (msg.type() == MessageType::ClientConnectReqPing) {
				auto it = getBestEdge();
				if (it != m_AliveEdges.end() && it->m_Score >= config::SCALE_UP_THRESHOLD)
					spawnEdge();
				if (it != m_AliveEdges.end()) {
					char ip_buf[INET_ADDRSTRLEN];
					inet_ntop(AF_INET, &it->m_Peer.sendAddr.sin_addr, ip_buf, INET_ADDRSTRLEN);
					uint16_t edge_port = ntohs(it->m_Peer.sendAddr.sin_port);

					Message response = Message::Builder()
						.type(MessageType::ClientConnectReqPong)
						.payload(std::string(ip_buf) + ":" + std::to_string(edge_port))
						.sequence(msg.sequence())
						.build();
					m_Adapter.enqueue(response, m_Adapter.setupPeer(ip, port));
				} else {
					std::cerr << "[Controller] No known edges to allocate client to." << std::endl;
				}
			}
		});

		uint16_t port = 4000;
		if (char *e = std::getenv("PORT")) port = std::atoi(e);
		m_Adapter.start("0.0.0.0", port);
		std::cout << "[Controller] Listening on port " << port << std::endl;

		while (true) {
			std::this_thread::sleep_for(std::chrono::seconds(2));
			for (auto &edge : m_AliveEdges) {
				edge.m_LastPingSentAt = std::chrono::system_clock::now();
				m_Adapter.enqueue(Message::Builder().type(MessageType::Heartbeat).build(), edge.m_Peer);
			}
			checkScaleDown();
		}
	}

  private:
	std::vector<Edge>::iterator getEdge(const std::string &ip, uint16_t port) {
		return std::find_if(m_AliveEdges.begin(), m_AliveEdges.end(), [&](const Edge &p) {
			return p.m_Peer.sendAddr.sin_port == htons(port) &&
				   p.m_Peer.sendAddr.sin_addr.s_addr == inet_addr(ip.c_str());
		});
	}

	std::vector<Edge>::iterator getBestEdge() {
		return std::min_element(m_AliveEdges.begin(), m_AliveEdges.end(), [](const Edge &a, const Edge &b) {
			return a.m_Score < b.m_Score;
		});
	}

	void removeDeadEdges() {
		std::erase_if(m_AliveEdges, [](const Edge &p) {
			return std::chrono::system_clock::now() - p.m_LastAckAt >
				   std::chrono::seconds(orla::config::HEARTBEAT_TIMEOUT_S);
		});
	}

	void spawnEdge() {
		std::string cmd = "docker run -d --network orla_orla_net "
						  "-e ROLE=edge -e PORT=" + std::to_string(m_NextEdgePort++) + " orla:latest";
		std::system(cmd.c_str());
		std::cout << "[Controller] Spawned new edge on port " << m_NextEdgePort - 1 << std::endl;
		m_ActiveEdges->Set(m_AliveEdges.size());
		m_ScaleUp->Increment();
	}

	void killEdge(std::vector<Edge>::iterator it) {
		uint16_t port    = ntohs(it->m_Peer.sendAddr.sin_port);
		std::string cmd  = "docker stop $(docker ps -q --filter publish=" + std::to_string(port) + ") 2>/dev/null";
		std::system(cmd.c_str());
		std::cout << "[Controller] Killed edge on port " << port << std::endl;
		m_AliveEdges.erase(it);
		m_ActiveEdges->Set(m_AliveEdges.size());
		m_ScaleDown->Increment();
	}

	void checkScaleDown() {
		if (m_AliveEdges.size() <= 1) return;
		bool allIdle = std::all_of(m_AliveEdges.begin(), m_AliveEdges.end(), [](const Edge &e) {
			return e.m_Score < config::SCALE_DOWN_THRESHOLD;
		});
		if (!allIdle) return;
		auto it = std::min_element(m_AliveEdges.begin(), m_AliveEdges.end(), [](const Edge &a, const Edge &b) {
			return a.m_Score < b.m_Score;
		});
		killEdge(it);
	}

	JuntosAdapter m_Adapter{};
	std::vector<Edge> m_AliveEdges{};
	uint16_t m_NextEdgePort = 4001;

	prometheus::Gauge* m_ActiveEdges = nullptr;
	prometheus::Counter* m_ScaleUp = nullptr;
	prometheus::Counter* m_ScaleDown = nullptr;

};
} // namespace orla
