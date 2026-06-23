#pragma once
#include "node_interface.h"
#include <network_adapter.h>
#include <random>

namespace orla {

class Client : public NodeInterface {
  public:
	explicit Client(prometheus::Registry &registry) : NodeInterface(registry) {}

	void run() override;

  private:
	void sendWorkRequest(int seq);

	JuntosAdapter m_Adapter;
	Peer *m_WorkerEdge = nullptr;
	uint32_t m_NextTaskId = 1;
	std::mt19937 m_Rng{std::random_device{}()};
	std::uniform_int_distribution<uint32_t> m_TaskDurationDistribution{200, 2000};
};

} // namespace orla
