#pragma once

#include <string>

namespace orla {

class NodeInterface {
  public:
	virtual ~NodeInterface() = default;
	virtual void run() = 0;
};

} // namespace orla
