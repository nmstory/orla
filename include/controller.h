#pragma once
#include "node_interface.h"
#include <chrono>
#include <message.h>
#include <mutex>
#include <network_adapter.h>
#include <prometheus/counter.h>
#include <prometheus/gauge.h>
#include <vector>

namespace orla {

struct Edge {
	Edge(Peer peer) : m_Peer(std::move(peer)) {}
	Edge(const Edge &) = default;
	Edge &operator=(Edge &&other) noexcept {
		m_Peer = std::move(other.m_Peer);
		m_Score = other.m_Score;
		m_LastPingSentAt = std::move(other.m_LastPingSentAt);
		m_LastAckAt = std::move(other.m_LastAckAt);
		return *this;
	}

	Peer m_Peer;
	float m_Score = 0.0f;
	std::chrono::system_clock::time_point m_LastPingSentAt{std::chrono::system_clock::now()};
	std::chrono::system_clock::time_point m_LastAckAt{std::chrono::system_clock::now()};
};

class Controller : public NodeInterface {
  public:
	explicit Controller(prometheus::Registry &registry);

	void run() override;

  private:
	std::vector<Edge>::iterator getEdge(const std::string &ip, uint16_t port);
	std::vector<Edge>::iterator getBestEdge();
	void removeDeadEdges();
	void spawnEdge();
	void killEdge(std::vector<Edge>::iterator it);
	void checkScaleDown();

	JuntosAdapter m_Adapter{};
	std::vector<Edge> m_AliveEdges{};
	uint16_t m_NextEdgePort = 4001;

	prometheus::Gauge *m_ActiveEdges = nullptr;
	prometheus::Counter *m_ScaleUp = nullptr;
	prometheus::Counter *m_ScaleDown = nullptr;

	mutable std::mutex m_EdgesMutex;
};
} // namespace orla
