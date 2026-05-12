#pragma once
#include <chrono>

namespace orla::config {
	constexpr std::chrono::seconds HEARTBEAT_TIMEOUT_S(10);

	// Routing score = LOAD_WEIGHT * clientCount + LATENCY_WEIGHT * latencyMs
	constexpr float LOAD_WEIGHT        = 1.0f;
	constexpr float LATENCY_WEIGHT     = 0.05f;
	constexpr float SCALE_UP_THRESHOLD = 10.0f; // spawn a new edge when best score exceeds this

	inline double LOSS_RATE;
	inline int MAX_LATENCY_MS;
	inline double REORDER_PROB;
}
