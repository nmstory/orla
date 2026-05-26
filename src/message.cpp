#include <message.h>

namespace orla {

Message::Message(MessageType t, std::string payload, uint32_t seq)
	: m_Header{seq, static_cast<uint32_t>(payload.size()), 0, t},
	  m_Payload(std::move(payload)) {
	m_Header.checksum =
		crc32(0, reinterpret_cast<const Bytef *>(m_Payload.data()),
			  m_Payload.size());
}

std::vector<uint8_t> Message::serialize() const {
	std::vector<uint8_t> buf(sizeof(m_Header) + m_Payload.size());
	std::memcpy(buf.data(), &m_Header, sizeof(m_Header));
	std::memcpy(buf.data() + sizeof(m_Header), m_Payload.data(),
				m_Payload.size());
	return buf;
}

Message Message::deserialize(const uint8_t *data, size_t len) {
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

Message Message::Builder::build() {
	return Message(m_Type, std::move(m_Payload), m_Sequence);
}

} // namespace orla
