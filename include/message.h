#pragma once
#include <common.h>

namespace orla {
enum class MessageType : uint8_t { Invalid = 0,
								   ClientConnectReqPing,
								   ClientConnectReqPong,
								   ClientAssigned,
								   ClientWorkRequest,
								   ClientWorkResult,
								   Data,
								   Heartbeat,
								   HeartbeatAck};

#pragma pack(push, 1)
struct WorkRequest {
	const uint32_t task_id;      // client-assigned, for correlation
	const uint32_t duration_ms;  // simulated compute time
};

struct WorkResult {
	uint32_t task_id;      // echoed back from WorkRequest
};
#pragma pack(pop)

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
		: m_Header{seq, static_cast<uint32_t>(payload.size()), 0, t},
		  m_Payload(std::move(payload)) {
		m_Header.checksum =
			crc32(0, reinterpret_cast<const Bytef *>(m_Payload.data()),
				  m_Payload.size());
	}

	MessageType type() const noexcept { return m_Header.type; }
	uint32_t sequence() const noexcept { return m_Header.sequence; }
	const std::string &payload() const noexcept { return m_Payload; }

	std::vector<uint8_t> serialize() const {
		std::vector<uint8_t> buf(sizeof(m_Header) + m_Payload.size());
		std::memcpy(buf.data(), &m_Header, sizeof(m_Header));
		std::memcpy(buf.data() + sizeof(m_Header), m_Payload.data(),
					m_Payload.size());
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
		MessageType m_Type = MessageType::ClientConnectReqPing;
		std::string m_Payload;
		uint32_t m_Sequence = 0;

	  public:
		Builder &type(MessageType t) {
			m_Type = t;
			return *this;
		}
		Builder &payload(std::string p) {
			m_Payload = std::move(p);
			return *this;
		}
		Builder &sequence(uint32_t s) {
			m_Sequence = s;
			return *this;
		}
		Message build() {
			return Message(m_Type, std::move(m_Payload), m_Sequence);
		}
	};

  private:
	MessageHeader m_Header{0, 0, 0, MessageType::Invalid};
	std::string m_Payload;
};

} // namespace orla
