#pragma once
#include <chrono>

namespace orla::config {
constexpr std::chrono::seconds HEARTBEAT_TIMEOUT_S(10);

// Routing score = LOAD_WEIGHT * clientCount + LATENCY_WEIGHT * latencyMs
constexpr float LOAD_WEIGHT = 1.0f;
constexpr float LATENCY_WEIGHT = 0.05f;
constexpr float SCALE_UP_THRESHOLD = 3.0f;	 // spawn a new edge when best score exceeds this
constexpr float SCALE_DOWN_THRESHOLD = 3.0f; // kill an edge when all scores fall below this

inline double LOSS_RATE;
inline int MAX_LATENCY_MS;
inline double REORDER_PROB;
} // namespace orla::config
