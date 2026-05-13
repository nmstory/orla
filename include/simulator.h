#pragma once
#include "metrics_registry.h"
#include "node_interface.h"
#include <client.h>
#include <controller.h>
#include <edge_node.h>
#include <iostream>
#include <memory>

namespace orla {

class Simulator {
  public:
	Simulator() = default;
	~Simulator() = default;

	int init() {
		uint16_t promPort = 9100;
		if (const char* p = std::getenv("PROMETHEUS_PORT"))
			promPort = static_cast<uint16_t>(std::stoi(p));
		m_Metrics = std::make_unique<MetricsRegistry>(promPort);

		std::string role = std::getenv("ROLE") ? std::getenv("ROLE") : "edge";
		std::unique_ptr<NodeInterface> node;

		if (role == "controller") {
			this->node = std::make_unique<Controller>();
		} else if (role == "edge") {
			this->node = std::make_unique<EdgeNode>();
		} else if (role == "client") {
			this->node = std::make_unique<Client>();
		} else {
			std::cerr << "Unknown ROLE: " << role << std::endl;
			return 1;
		}
		return 0;
	}

	void run() {
		if (this->node) {
			this->node->run();
		} else {
			std::cerr << "Simulator not initialized with a node." << std::endl;
		}
	}

  private:
	std::unique_ptr<MetricsRegistry> m_Metrics;
	std::unique_ptr<NodeInterface> node;
};

} // namespace orla