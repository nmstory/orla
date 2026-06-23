#pragma once
#include "node_interface.h"
#include <network_adapter.h>
#include <prometheus/counter.h>
#include <prometheus/gauge.h>
#include <prometheus/histogram.h>
#include <vector>

namespace orla {

class Message;

class EdgeNode : public NodeInterface {
  public:
	explicit EdgeNode(prometheus::Registry &registry);

	void run() override;

  private:
	void processWorkRequest(const Message &msg, Peer client);

	JuntosAdapter m_Adapter;
	std::vector<Peer> m_Clients;

	prometheus::Counter *m_TasksCompleted = nullptr;
	prometheus::Histogram *m_TaskDuration = nullptr;
	prometheus::Gauge *m_ActiveClients = nullptr;
};

} // namespace orla
