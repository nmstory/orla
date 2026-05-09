#pragma once
#include <chrono>

namespace orla::config {
	constexpr std::chrono::seconds HEARTBEAT_TIMEOUT_S(10); // seconds

	inline double LOSS_RATE;
	inline int MAX_LATENCY_MS;
	inline double REORDER_PROB;
}
