#pragma once

#include <string>

namespace orla {

class NodeInterface {
  public:
	virtual ~NodeInterface() = default;

	// TODO: split "start" and "run" logic
	virtual void run() = 0;

	// TODO: pull member variables up to NodeInterface
};

} // namespace orla
