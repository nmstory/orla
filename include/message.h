#include <common.h>

namespace orla {
enum class MessageType : uint8_t { Ping = 0,
								   Pong,
								   Data,
								   Heartbeat };

#pragma pack(push, 1)
struct MessageHeader {
	uint32_t sequence;	   // incremental sequence number
	uint32_t payload_size; // payload size in bytes
	uint32_t checksum;	   // CRC32 of payload
	MessageType type;	   // message type
};
#pragma pack(pop)

class Message {
  public:
	Message(MessageType t, std::string payload, uint32_t seq = 0)
		: header_{seq, static_cast<uint32_t>(payload.size()), 0, t},
		  payload_(std::move(payload)) {
		header_.checksum =
			crc32(0, reinterpret_cast<const Bytef *>(payload_.data()),
				  payload_.size());
	}

	MessageType type() const noexcept { return header_.type; }
	uint32_t sequence() const noexcept { return header_.sequence; }
	const std::string &payload() const noexcept { return payload_; }

	std::vector<uint8_t> serialize() const {
		std::vector<uint8_t> buf(sizeof(header_) + payload_.size());
		std::memcpy(buf.data(), &header_, sizeof(header_));
		std::memcpy(buf.data() + sizeof(header_), payload_.data(),
					payload_.size());
		return buf;
	}

	static Message deserialize(const uint8_t *data, size_t len) {
		if (len < sizeof(MessageHeader)) {
			throw std::runtime_error("Too short");
		}
		MessageHeader hdr;
		std::memcpy(&hdr, data, sizeof(MessageHeader));
		std::string payload;
		if (hdr.payload_size > 0) {
			if (len < sizeof(MessageHeader) + hdr.payload_size) {
				throw std::runtime_error("Payload size mismatch");
			}
			payload.assign(
				reinterpret_cast<const char *>(data + sizeof(MessageHeader)),
				hdr.payload_size);
			uint32_t crc =
				crc32(0, reinterpret_cast<const Bytef *>(payload.data()),
					  payload.size());
			if (crc != hdr.checksum)
				throw std::runtime_error("CRC mismatch");
		}
		return Message(hdr.type, payload, hdr.sequence);
	}

	// Using builder pattern for easier construction
	class Builder {
		MessageType type_ = MessageType::Ping;
		std::string payload_;
		uint32_t sequence_ = 0;

	  public:
		Builder &type(MessageType t) {
			type_ = t;
			return *this;
		}
		Builder &payload(std::string p) {
			payload_ = std::move(p);
			return *this;
		}
		Builder &sequence(uint32_t s) {
			sequence_ = s;
			return *this;
		}
		Message build() {
			return Message(type_, std::move(payload_), sequence_);
		}
	};

  private:
	MessageHeader header_{0, 0, 0, MessageType::Ping};
	std::string payload_;
};

} // namespace orla