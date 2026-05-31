#pragma once
#include "../external/juntos/include/network_handler.h"
#include "../external/juntos/include/session_linux.h"
#include "message.h"
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <prometheus/counter.h>
#include <prometheus/registry.h>
#include <queue>
#include <random>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <network_handler.h>
#include <sys/socket.h>

namespace orla {

class INetworkAdapter {
  public:
	virtual ~INetworkAdapter() = default;
	virtual void enqueue(const Message &msg, const Peer &peer) = 0;
	virtual void on_receive(
		std::function<void(const Message &, const std::string &, uint16_t)> callback) = 0;
	virtual void start(std::string hostname, uint16_t port) = 0;
};

class JuntosAdapter : public INetworkAdapter {
  public:
	JuntosAdapter() = default;
	~JuntosAdapter() = default;

	void enqueue(const Message &msg, const Peer &peer) override;
	void on_receive(
		std::function<void(const Message &, const std::string &, uint16_t)> callback) override;
	void start(std::string hostname, uint16_t port) override;

	void initMetrics(prometheus::Registry& registry);
	Peer setupPeer(const std::string &peer_addr, uint16_t port);

  private:
	void workerLoop(std::stop_token st);

	std::function<void(const Message &, const std::string &, uint16_t)> m_RecvCallback;
	std::unique_ptr<LinuxSession> m_Session;

	struct Pending {
		std::chrono::steady_clock::time_point when;
		sockaddr_in addr;
		std::vector<uint8_t> buf;
		bool operator>(Pending const &o) const { return when > o.when; }
	};

	std::priority_queue<Pending, std::vector<Pending>, std::greater<Pending>> m_Pending;
	std::mutex m_Mutex;
	std::condition_variable_any m_Cv;

	prometheus::Counter* m_TotalBytesSent = nullptr;
	prometheus::Counter* m_TotalBytesReceived = nullptr;
	prometheus::Counter* m_PacketsDropped = nullptr;

	/* jthreads declared last so they're destructed first, while
	   the resources they touch (m_Session, m_Cv etc.) are still alive */
	std::jthread m_Worker;
	std::jthread m_RecvThread;
};

} // namespace orla
