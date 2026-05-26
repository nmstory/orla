#include <simulator.h>

#include <client.h>
#include <controller.h>
#include <cstdlib>
#include <edge_node.h>
#include <iostream>

namespace orla {

int Simulator::init() {
	uint16_t promPort = 9100;
	if (const char* p = std::getenv("PROMETHEUS_PORT"))
		promPort = static_cast<uint16_t>(std::stoi(p));
	m_Metrics = std::make_unique<MetricsRegistry>(promPort);

	std::string role = std::getenv("ROLE") ? std::getenv("ROLE") : "edge";
	std::unique_ptr<NodeInterface> node;

	if (role == "controller") {
		this->node = std::make_unique<Controller>(m_Metrics->registry());
	} else if (role == "edge") {
		this->node = std::make_unique<EdgeNode>(m_Metrics->registry());
	} else if (role == "client") {
		this->node = std::make_unique<Client>(m_Metrics->registry());
	} else {
		std::cerr << "Unknown ROLE: " << role << std::endl;
		return 1;
	}
	return 0;
}

void Simulator::run() {
	if (this->node) {
		this->node->run();
	} else {
		std::cerr << "Simulator not initialized with a node." << std::endl;
	}
}

} // namespace orla
