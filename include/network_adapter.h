#pragma once
#include "../external/juntos/include/network_handler.h"
#include "../external/juntos/include/session_linux.h"
#include "config.h"
#include "message.h"
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <random>
#include <span>
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
	JuntosAdapter() : session_(nullptr),
					  rng_(std::random_device{}()), dist01_(0.0, 1.0), stopped_(false) {}

	void enqueue(const Message &msg, const Peer &peer) override {
		auto buf = msg.serialize();
		std::vector<uint8_t> raw(buf.begin(), buf.end());

		if (dist01_(rng_) < config::LOSS_RATE) {
			return; // packet dropped
		}

		// add random latency
		int ms = config::MAX_LATENCY_MS > 0 ? std::uniform_int_distribution<int>(0, config::MAX_LATENCY_MS)(rng_) : 0;
		auto deliver_at = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);

		{ // push to pending queue
			std::lock_guard<std::mutex> lk(mutex_);
			pending_.push(Pending{deliver_at, peer.sendAddr, std::move(raw)});
			cv_.notify_one();
		}
	}

	void on_receive(
		std::function<void(const Message &, const std::string &, uint16_t)> callback) override {
		recv_callback_ = std::move(callback);
	}

	void start(std::string hostname, uint16_t port) override {
		if (char *e = std::getenv("LOSS_RATE"))
			config::LOSS_RATE = std::stod(e);
		if (char *e = std::getenv("MAX_LATENCY_MS"))
			config::MAX_LATENCY_MS = std::atoi(e);
		if (char *e = std::getenv("REORDER_PROB"))
			config::REORDER_PROB = std::stod(e);

		if (!session_) {
			session_ = new LinuxSession();
		}
		session_->initSessionSolo(hostname, port);

		auto receive_loop = [this]() {
			while (!stopped_) {
				auto [success, data, sender] = recvData(session_->getSocketFD());
				if (success && recv_callback_) {
					std::cout << "Received " << data.size() << std::endl;
					try{
						Message msg = Message::deserialize(reinterpret_cast<const uint8_t *>(data.data()), data.size());
						char ip[INET_ADDRSTRLEN];
						inet_ntop(AF_INET, &(sender.sin_addr), ip, INET_ADDRSTRLEN);
						uint16_t sender_port = ntohs(sender.sin_port);
						recv_callback_(msg, std::string(ip), sender_port);
					} catch (const std::exception &e){
						std::cerr << "Deserialise failed: " << e.what() << std::endl;
					}
				}
				std::this_thread::yield(); // TODO: cleanup blocking tech
			}
		};
		std::thread(receive_loop).detach();

		// sender worker
		worker_ = std::thread([this] { this->workerLoop(); });
	}

	Peer setupPeer(const std::string &peer_addr, uint16_t port) {
		return session_->setupPeer(peer_addr, port);
	}

	~JuntosAdapter() {
		// stop worker
		{
			std::lock_guard<std::mutex> lk(mutex_);
			stopped_ = true;
			cv_.notify_one();
		}
		if (worker_.joinable())
			worker_.join();
		delete session_;
	}

  private:
	std::function<void(const Message &, const std::string &, uint16_t)> recv_callback_;
	LinuxSession *session_;

	std::mt19937 rng_;
	std::uniform_real_distribution<double> dist01_;

	struct Pending {
		std::chrono::steady_clock::time_point when;
		sockaddr_in addr;
		std::vector<uint8_t> buf;
		bool operator>(Pending const &o) const { return when > o.when; } // for priority_queue
	};

	std::priority_queue<Pending, std::vector<Pending>, std::greater<Pending>> pending_;
	std::mutex mutex_;
	std::condition_variable cv_;
	std::thread worker_;
	bool stopped_;

	void workerLoop() {
		std::unique_lock<std::mutex> lk(mutex_);
		while (!stopped_) {
			if (pending_.empty()) {
				cv_.wait(lk, [this] { return stopped_ || !pending_.empty(); });
				if (stopped_)
					break;
			}

			auto &pkt = pending_.top();
			auto now = std::chrono::steady_clock::now();
			if (pkt.when > now)
				cv_.wait_until(lk, pkt.when);
			else {
				Pending send_pkt = std::move(const_cast<Pending &>(pending_.top()));
				pending_.pop();
				lk.unlock();
				if (session_) {
					int sock = session_->getSocketFD();
					std::span<const std::byte> payload(reinterpret_cast<const std::byte *>(send_pkt.buf.data()), send_pkt.buf.size());
					char dest_ip[INET_ADDRSTRLEN];
					inet_ntop(AF_INET, &send_pkt.addr.sin_addr, dest_ip, INET_ADDRSTRLEN);
					std::cout << "[Worker] Sending " << payload.size() << " bytes to "
							<< dest_ip << ":" << ntohs(send_pkt.addr.sin_port)
							<< " on sock " << sock << std::endl;
					sendData<int>(sock, send_pkt.addr, payload, payload.size());

					sendData<int>(sock, send_pkt.addr, payload, payload.size());
				}
				lk.lock();
			}
		}
	}
};

} // namespace orla
