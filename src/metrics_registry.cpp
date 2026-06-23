#include <metrics_registry.h>

namespace orla {

MetricsRegistry::MetricsRegistry(uint16_t port)
	: m_Exposer("0.0.0.0:" + std::to_string(port)),
	  m_Registry(std::make_shared<prometheus::Registry>()) {
	m_Exposer.RegisterCollectable(m_Registry);
}

} // namespace orla
