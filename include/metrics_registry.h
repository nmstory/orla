#pragma once

#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <memory>
#include <string>

namespace orla {

class MetricsRegistry {
  public:
    explicit MetricsRegistry(uint16_t port)
        : m_Exposer("0.0.0.0:" + std::to_string(port)),
          m_Registry(std::make_shared<prometheus::Registry>()) {
        m_Exposer.RegisterCollectable(m_Registry);
    }

    prometheus::Registry& registry() { return *m_Registry; }

  private:
    prometheus::Exposer m_Exposer;
    std::shared_ptr<prometheus::Registry> m_Registry;
};

} // namespace orla
