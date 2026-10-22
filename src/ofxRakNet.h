#pragma once

// ofxRakNet: RakNet networking for openFrameworks, built on SLikeNet (this
// branch: Sabrosa's maintained fork, https://gitlab.com/Sabrosa/slikenet), with
// security and macOS fixes (see libs/slikenet/PATCHES.md).
//
// ofxRakNet::Peer wraps a RakPeerInterface: start it as a server or a client,
// send messages, and get connection / message events from update(), on the
// main thread. getRakPeer() gives the full SLikeNet API for anything else.

#include "ofEvents.h"
#include "ofFileUtils.h"
#include "ofJson.h"
#include "ofxRakNetCrypto.h"

#include <cstdint>
#include <string>
#include <vector>

namespace SLNet {
class RakPeerInterface;
struct Packet;
}

namespace ofxRakNet {

/// A connected system, stable for the connection's lifetime (RakNet's GUID).
using PeerId = uint64_t;

/// How a message is delivered (RakNet's PacketReliability).
enum class Reliability {
	Unreliable,          ///< may be lost, duplicated or out of order (positions, cursors)
	UnreliableSequenced, ///< may be lost; older ones are dropped
	Reliable,            ///< arrives, in any order
	ReliableOrdered,     ///< arrives, in the order sent (default; chat, state)
	ReliableSequenced,   ///< arrives; older ones are dropped
};

/// Send priority (RakNet's PacketPriority).
enum class Priority { Immediate, High, Medium, Low };

struct ConnectionEvent {
	PeerId peer = 0;
	std::string address; ///< "ip|port"
};

struct DisconnectionEvent {
	enum class Reason {
		Disconnected,        ///< the other side closed the connection
		Lost,                ///< timed out
		ConnectFailed,       ///< connect() got no answer
		ServerFull,
		InvalidPassword,
		Banned,
		IncompatibleProtocol,
		AlreadyConnected,
	};
	PeerId peer = 0;
	std::string address;
	Reason reason = Reason::Disconnected;
};

struct MessageEvent {
	PeerId peer = 0;
	std::string address;
	/// The type given to send() (0 .. MAX_MESSAGE_TYPE).
	uint8_t type = 0;
	/// The payload, without the type byte (decrypted if it was encrypted).
	ofBuffer data;
	/// The message was encrypted (see Peer::setPassphrase).
	bool encrypted = false;
	/// Encrypted, and decrypted and verified with our passphrase. False for
	/// plain messages, and for encrypted ones that failed (wrong passphrase or
	/// changed on the way; data is then empty).
	bool authentic = false;

	std::string getText() const { return data.getText(); }
	/// The payload parsed as JSON (null if it isn't).
	ofJson getJson() const;
};

std::string toString(DisconnectionEvent::Reason reason);

class Peer {
public:
	/// Largest message type: one byte after RakNet's reserved message IDs.
	static const uint8_t MAX_MESSAGE_TYPE;

	Peer();
	~Peer();
	Peer(const Peer &) = delete;
	Peer & operator=(const Peer &) = delete;

	/// Listens on port for up to maxConnections clients (and can also connect out).
	bool startServer(uint16_t port, unsigned int maxConnections = 32, const std::string & password = "");
	/// Starts without accepting connections, on any free port; then connect().
	bool startClient();
	/// Connects to a server; onConnect or onDisconnect (ConnectFailed, ...) follows.
	bool connect(const std::string & host, uint16_t port, const std::string & password = "");
	/// Closes one connection (the other side gets Disconnected).
	void disconnect(PeerId peer);
	/// Closes every connection and stops (also done by the destructor).
	void stop();

	bool isRunning() const;
	bool isServer() const { return server; }
	/// The port this peer is bound to (0 when not running).
	uint16_t getPort() const;

	/// Sends to one peer. type: 0 .. MAX_MESSAGE_TYPE, given back in MessageEvent::type.
	bool send(PeerId peer, uint8_t type, const void * data, size_t size,
		Reliability reliability = Reliability::ReliableOrdered, Priority priority = Priority::High, uint8_t channel = 0);
	bool send(PeerId peer, uint8_t type, const std::string & text,
		Reliability reliability = Reliability::ReliableOrdered, Priority priority = Priority::High, uint8_t channel = 0);
	bool send(PeerId peer, uint8_t type, const ofBuffer & buffer,
		Reliability reliability = Reliability::ReliableOrdered, Priority priority = Priority::High, uint8_t channel = 0);
	bool send(PeerId peer, uint8_t type, const ofJson & json,
		Reliability reliability = Reliability::ReliableOrdered, Priority priority = Priority::High, uint8_t channel = 0);

	/// Sends to every connected peer, except `except` (0: none).
	bool broadcast(uint8_t type, const void * data, size_t size, PeerId except = 0,
		Reliability reliability = Reliability::ReliableOrdered, Priority priority = Priority::High, uint8_t channel = 0);
	bool broadcast(uint8_t type, const std::string & text, PeerId except = 0,
		Reliability reliability = Reliability::ReliableOrdered, Priority priority = Priority::High, uint8_t channel = 0);
	bool broadcast(uint8_t type, const ofJson & json, PeerId except = 0,
		Reliability reliability = Reliability::ReliableOrdered, Priority priority = Priority::High, uint8_t channel = 0);

	/// Handles everything received since the last call and notifies the events.
	/// Call once per frame (or setAutoUpdate(true)).
	void update();
	/// Calls update() from ofEvents().update, before ofApp::update.
	void setAutoUpdate(bool autoUpdate);

	/// Encrypts every message sent with AES-256-GCM, under a key derived from
	/// the passphrase (PBKDF2-HMAC-SHA256, random salt), and decrypts what
	/// arrives (see ofxRakNetCrypto.h). Every peer needs the same passphrase.
	/// Empty turns it off. Deriving the key takes a moment (200000 iterations).
	void setPassphrase(const std::string & passphrase);
	bool isEncrypting() const { return cipher.isEnabled(); }
	/// The same on every peer with the same passphrase (compare to check).
	std::string getFingerprint() const { return cipher.getFingerprint(); }
	/// With a passphrase set: drop messages that aren't encrypted with it,
	/// instead of delivering them with authentic = false.
	bool requireEncryption = false;

	std::vector<PeerId> getConnections() const;
	size_t getNumConnections() const;
	std::string getAddress(PeerId peer) const;
	/// Average round trip in milliseconds, or -1.
	int getPing(PeerId peer) const;
	PeerId getMyId() const;

	ofEvent<ConnectionEvent> onConnect;
	ofEvent<DisconnectionEvent> onDisconnect;
	ofEvent<MessageEvent> onMessage;
	/// Every packet that isn't a connection event or an ofxRakNet message (other
	/// RakNet message IDs, plugins). The packet is only valid during the event.
	ofEvent<SLNet::Packet *> onPacket;

	/// The SLikeNet peer, for the rest of the RakNet API (plugins, statistics, ...).
	SLNet::RakPeerInterface * getRakPeer() const { return peer; }

private:
	SLNet::RakPeerInterface * peer = nullptr;
	bool server = false;
	ofEventListener updateListener;
	std::vector<char> sendBuffer;
	crypto::Cipher cipher;

	bool start(uint16_t port, unsigned int maxConnections);
	bool sendRaw(PeerId target, bool broadcast, uint8_t type, const void * data, size_t size,
		Reliability reliability, Priority priority, uint8_t channel);
};

} // namespace ofxRakNet
