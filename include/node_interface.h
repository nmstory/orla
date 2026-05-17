#pragma once

#include <prometheus/registry.h>
#include <string>

namespace orla {

class NodeInterface {
  public:
	explicit NodeInterface(prometheus::Registry& registry) : m_Registry(registry) {}
	virtual ~NodeInterface() = default;

	// TODO: split "start" and "run" logic
	virtual void run() = 0;

  protected:
	prometheus::Registry& m_Registry;

	// TODO: pull member variables up to NodeInterface
};

} // namespace orla
