#pragma once
#include "metrics_registry.h"
#include "node_interface.h"
#include <memory>

namespace orla {

class Simulator {
  public:
	Simulator() = default;
	~Simulator() = default;

	int init();
	void run();

  private:
	std::unique_ptr<MetricsRegistry> m_Metrics;
	std::unique_ptr<NodeInterface> node;
};

} // namespace orla
