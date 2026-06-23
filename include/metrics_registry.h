#pragma once

#include <memory>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <string>

namespace orla {

class MetricsRegistry {
  public:
	explicit MetricsRegistry(uint16_t port);

	prometheus::Registry &registry() { return *m_Registry; }

  private:
	prometheus::Exposer m_Exposer;
	std::shared_ptr<prometheus::Registry> m_Registry;
};

} // namespace orla
