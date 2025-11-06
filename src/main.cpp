#include <common.h>
#include <network_adapter.h>

using orla::Message;
using orla::MessageType;

int main() {
	Message msg = Message::Builder()
					  .type(MessageType::Data)
					  .payload("Hello, world!")
					  .sequence(42)
					  .build();
	orla::JuntosAdapter adapter;
	adapter.start("edge", 4000);
	Peer ctl = adapter.addPeer("controller", 4000);
	adapter.send(msg, ctl);
	return 0;
}