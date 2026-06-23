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
								   HeartbeatAck };

#pragma pack(push, 1)
struct WorkRequest {
	const uint32_t task_id;		// client-assigned, for correlation
	const uint32_t duration_ms; // simulated compute time
};

struct WorkResult {
	uint32_t task_id; // echoed back from WorkRequest
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
	Message(MessageType t, std::string payload, uint32_t seq = 0);

	MessageType type() const noexcept { return m_Header.type; }
	uint32_t sequence() const noexcept { return m_Header.sequence; }
	const std::string &payload() const noexcept { return m_Payload; }

	std::vector<uint8_t> serialize() const;

	static Message deserialize(const uint8_t *data, size_t len);

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
		Message build();
	};

  private:
	MessageHeader m_Header{0, 0, 0, MessageType::Invalid};
	std::string m_Payload;
};

} // namespace orla
