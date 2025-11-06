#pragma once
#include "../external/juntos/include/network_handler.h"
#include "../external/juntos/include/session_linux.h"
#include "message.h"
#include <functional>
#include <string>
#include <thread>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <network_handler.h>
#include <sys/socket.h>

namespace orla {

class INetworkAdapter {
  public:
	virtual ~INetworkAdapter() = default;
	virtual void send(const Message &msg, const Peer &peer) = 0;
	virtual void on_receive(
		std::function<void(const Message &, const std::string &, uint16_t)> callback) = 0;
	virtual void start(std::string hostname, uint16_t port) = 0;
};

class JuntosAdapter : public INetworkAdapter {
  public:
	JuntosAdapter() {}
	void send(const Message &msg, const Peer &peer) override {
		// TODO: need a check if the peer is known/initialised
		auto buf = msg.serialize();
		std::span<const std::byte> payload(reinterpret_cast<const std::byte *>(buf.data()), buf.size());
		sendData<int>(session_->getSocketFD(), peer.sendAddr, payload, payload.size());
	}
	void on_receive(
		std::function<void(const Message &, const std::string &, uint16_t)> callback) override {
		recv_callback_ = std::move(callback);
	}

	void start(std::string hostname, uint16_t port) override {
		// Setup socket, bind, etc.
		session_ = new LinuxSession();
		session_->initSessionSolo(hostname, port);
		auto receive_loop = [this]() {
			while (true) {
				auto [success, data, sender] =
					recvData(session_->getSocketFD());
				if (success && recv_callback_) {
					Message msg = Message::deserialize(reinterpret_cast<const uint8_t *>(data.data()), data.size());
					char ip[INET_ADDRSTRLEN];
					inet_ntop(AF_INET, &(sender.sin_addr), ip, INET_ADDRSTRLEN);
					uint16_t sender_port = ntohs(sender.sin_port);
					recv_callback_(msg, std::string(ip), sender_port);
				}
			}
		};
		// TODO: threading, i.e: std::thread(receive_loop).detach();
	}

	Peer addPeer(const std::string &peer_addr, uint16_t port) {
		return session_->addPeer(peer_addr, port);
	}

  private:
	std::function<void(const Message &, const std::string &, uint16_t)> recv_callback_;
	LinuxSession *session_;
};

} // namespace orla
