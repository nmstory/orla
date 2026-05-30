#include <controller.h>

#include <algorithm>
#include <config.h>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <thread>
#include <vector>

namespace orla {

Controller::Controller(prometheus::Registry& registry) : NodeInterface(registry) {
	m_ActiveEdges = &prometheus::BuildGauge()
		.Name("active_edges")
		.Help("The number of edges currently active within this session")
		.Register(m_Registry).Add({});

	auto& scaleFamily = prometheus::BuildCounter()
		.Name("scaling_events_total")
		.Help("Autoscaling events")
		.Register(m_Registry);

	m_ScaleUp   = &scaleFamily.Add({{"direction", "up"}});
	m_ScaleDown = &scaleFamily.Add({{"direction", "down"}});
}

void Controller::run() {
	m_Adapter.initMetrics(m_Registry);

	m_Adapter.on_receive([this](const Message &msg, const std::string &ip, uint16_t port) {
		if (msg.type() == MessageType::Heartbeat) {
			std::optional<Peer> probe_peer;
			{
				std::lock_guard<std::mutex> lk(m_EdgesMutex);
				auto it = getEdge(ip, port);
				if (it == m_AliveEdges.end()) {
					m_AliveEdges.push_back(m_Adapter.setupPeer(ip, port));
					it = m_AliveEdges.end() - 1;
					std::cout << "[Controller] Edge registered: " << ip << ":" << port << std::endl;
					it->m_LastPingSentAt = std::chrono::system_clock::now();
					probe_peer.emplace(it->m_Peer);
				}
				removeDeadEdges();
				m_ActiveEdges->Set(m_AliveEdges.size());
			}
			if (probe_peer) {
				m_Adapter.enqueue(Message::Builder().type(MessageType::Heartbeat).build(), *probe_peer);
			}
		} else if (msg.type() == MessageType::HeartbeatAck) {
			std::lock_guard<std::mutex> lk(m_EdgesMutex);
			auto it = getEdge(ip, port);
			if (it != m_AliveEdges.end()) {
				int64_t latencyMs = std::chrono::duration_cast<std::chrono::milliseconds>(
					std::chrono::system_clock::now() - it->m_LastPingSentAt).count();
				uint8_t clientCount = std::stoi(msg.payload());
				it->m_Score     = config::LOAD_WEIGHT * clientCount + config::LATENCY_WEIGHT * latencyMs;
				it->m_LastAckAt = std::chrono::system_clock::now();
			}
		} else if (msg.type() == MessageType::ClientConnectReqPing) {
			std::optional<Peer> best_peer;
			{
				std::lock_guard<std::mutex> lk(m_EdgesMutex);
				auto it = getBestEdge();
				if (it != m_AliveEdges.end()) {
					best_peer.emplace(it->m_Peer);
					if (it->m_Score >= config::SCALE_UP_THRESHOLD) {
						spawnEdge();
					}
				}
			}
			if (best_peer) {
				char ip_buf[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, &best_peer->sendAddr.sin_addr, ip_buf, INET_ADDRSTRLEN);
				uint16_t edge_port = ntohs(best_peer->sendAddr.sin_port);

				Message response = Message::Builder()
					.type(MessageType::ClientConnectReqPong)
					.payload(std::string(ip_buf) + ":" + std::to_string(edge_port))
					.sequence(msg.sequence())
					.build();
				m_Adapter.enqueue(response, m_Adapter.setupPeer(ip, port));
			} else {
				std::cerr << "[Controller] No known edges to allocate client to." << std::endl;
			}
		}
	});

	uint16_t port = 4000;
	if (char *e = std::getenv("PORT")) port = std::atoi(e);
	m_Adapter.start("0.0.0.0", port);
	std::cout << "[Controller] Listening on port " << port << std::endl;

	while (true) {
		std::this_thread::sleep_for(std::chrono::seconds(2));

		std::vector<Peer> targets;
		{
			std::lock_guard<std::mutex> lk(m_EdgesMutex);
			targets.reserve(m_AliveEdges.size());
			for (auto &edge : m_AliveEdges) {
				edge.m_LastPingSentAt = std::chrono::system_clock::now();
				targets.push_back(edge.m_Peer);
			}
			checkScaleDown();
		}

		for (auto &peer : targets) {
			m_Adapter.enqueue(Message::Builder().type(MessageType::Heartbeat).build(), peer);
		}
	}
}

// Helpers below assume m_EdgesMutex is held by the caller.

std::vector<Edge>::iterator Controller::getEdge(const std::string &ip, uint16_t port) {
	return std::find_if(m_AliveEdges.begin(), m_AliveEdges.end(), [&](const Edge &p) {
		return p.m_Peer.sendAddr.sin_port == htons(port) &&
			   p.m_Peer.sendAddr.sin_addr.s_addr == inet_addr(ip.c_str());
	});
}

std::vector<Edge>::iterator Controller::getBestEdge() {
	return std::min_element(m_AliveEdges.begin(), m_AliveEdges.end(), [](const Edge &a, const Edge &b) {
		return a.m_Score < b.m_Score;
	});
}

void Controller::removeDeadEdges() {
	std::erase_if(m_AliveEdges, [](const Edge &p) {
		return std::chrono::system_clock::now() - p.m_LastAckAt >
			   std::chrono::seconds(orla::config::HEARTBEAT_TIMEOUT_S);
	});
}

void Controller::spawnEdge() {
	uint16_t port = m_NextEdgePort++;
	std::string name = "edge-" + std::to_string(port - 4000);
	std::string cmd = "docker run -d --network orla_orla_net "
					  "--name " + name + " "
					  "--label orla.role=edge "
					  "-e ROLE=edge -e PORT=" + std::to_string(port) +
					  " -e PROMETHEUS_PORT=9101 orla:latest";
	std::cout << "[Controller] Spawning edge on port " << port << "..." << std::endl;
	std::thread([cmd, port]() {
		if (std::system(cmd.c_str()) != 0)
			std::cerr << "[Controller] Failed to spawn edge on port " << port << std::endl;
	}).detach();
	m_ScaleUp->Increment();
}

void Controller::killEdge(std::vector<Edge>::iterator it) {
	uint16_t port = ntohs(it->m_Peer.sendAddr.sin_port);
	m_AliveEdges.erase(it);
	m_ActiveEdges->Set(m_AliveEdges.size());
	m_ScaleDown->Increment();

	// docker stop blocks for seconds so not running it under the lock
	std::thread([port]() {
		std::string cmd = "docker stop $(docker ps -q --filter publish=" + std::to_string(port) + ") 2>/dev/null";
		if (std::system(cmd.c_str()) != 0)
			std::cerr << "[Controller] docker stop failed on port " << port << std::endl;
		std::cout << "[Controller] Killed edge on port " << port << std::endl;
	}).detach();
}

void Controller::checkScaleDown() {
	if (m_AliveEdges.size() <= 1) return;
	bool allIdle = std::all_of(m_AliveEdges.begin(), m_AliveEdges.end(), [](const Edge &e) {
		return e.m_Score < config::SCALE_DOWN_THRESHOLD;
	});
	if (!allIdle) return;
	auto it = std::min_element(m_AliveEdges.begin(), m_AliveEdges.end(), [](const Edge &a, const Edge &b) {
		return a.m_Score < b.m_Score;
	});
	killEdge(it);
}

} // namespace orla
