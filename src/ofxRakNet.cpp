#include "ofxRakNet.h"

#include "ofLog.h"

#include "slikenet/peerinterface.h"
#include "slikenet/MessageIdentifiers.h"
#include "slikenet/types.h"
#include "slikenet/PacketPriority.h"

#include <cstring>

#if defined(_MSC_VER)
#pragma comment(lib, "ws2_32.lib") // Winsock; SLikeNet links IPHLPAPI and Winmm itself
#endif

namespace ofxRakNet {

namespace {

PacketReliability toRakNet(Reliability r) {
	switch (r) {
	case Reliability::Unreliable: return UNRELIABLE;
	case Reliability::UnreliableSequenced: return UNRELIABLE_SEQUENCED;
	case Reliability::Reliable: return RELIABLE;
	case Reliability::ReliableSequenced: return RELIABLE_SEQUENCED;
	case Reliability::ReliableOrdered: default: return RELIABLE_ORDERED;
	}
}

PacketPriority toRakNet(Priority p) {
	switch (p) {
	case Priority::Immediate: return IMMEDIATE_PRIORITY;
	case Priority::Medium: return MEDIUM_PRIORITY;
	case Priority::Low: return LOW_PRIORITY;
	case Priority::High: default: return HIGH_PRIORITY;
	}
}

std::string addressString(const SLNet::SystemAddress & address) {
	return address.ToString(true);
}

} // namespace

const uint8_t Peer::MAX_MESSAGE_TYPE = static_cast<uint8_t>(255 - ID_USER_PACKET_ENUM);

ofJson MessageEvent::getJson() const {
	return ofJson::parse(data.getText(), nullptr, false);
}

std::string toString(DisconnectionEvent::Reason reason) {
	switch (reason) {
	case DisconnectionEvent::Reason::Disconnected: return "disconnected";
	case DisconnectionEvent::Reason::Lost: return "connection lost";
	case DisconnectionEvent::Reason::ConnectFailed: return "connection attempt failed";
	case DisconnectionEvent::Reason::ServerFull: return "server full";
	case DisconnectionEvent::Reason::InvalidPassword: return "invalid password";
	case DisconnectionEvent::Reason::Banned: return "banned";
	case DisconnectionEvent::Reason::IncompatibleProtocol: return "incompatible protocol";
	case DisconnectionEvent::Reason::AlreadyConnected: return "already connected";
	}
	return "unknown";
}

Peer::Peer() = default;

Peer::~Peer() {
	stop();
}

bool Peer::start(uint16_t port, unsigned int maxConnections) {
	stop();
	peer = SLNet::RakPeerInterface::GetInstance();
	SLNet::SocketDescriptor socket(port, nullptr);
	auto result = peer->Startup(maxConnections, &socket, 1);
	if (result != SLNet::RAKNET_STARTED) {
		ofLogError("ofxRakNet") << "couldn't start on port " << port << " (SLikeNet StartupResult " << result << ")";
		SLNet::RakPeerInterface::DestroyInstance(peer);
		peer = nullptr;
		return false;
	}
	return true;
}

bool Peer::startServer(uint16_t port, unsigned int maxConnections, const std::string & password) {
	if (!start(port, maxConnections)) {
		return false;
	}
	server = true;
	peer->SetMaximumIncomingConnections(static_cast<unsigned short>(maxConnections));
	if (!password.empty()) {
		peer->SetIncomingPassword(password.data(), static_cast<int>(password.size()));
	}
	return true;
}

bool Peer::startClient() {
	if (!start(0, 8)) {
		return false;
	}
	server = false;
	return true;
}

bool Peer::connect(const std::string & host, uint16_t port, const std::string & password) {
	if (!peer && !startClient()) {
		return false;
	}
	auto result = peer->Connect(host.c_str(), port,
		password.empty() ? nullptr : password.data(), static_cast<int>(password.size()));
	if (result != SLNet::CONNECTION_ATTEMPT_STARTED) {
		ofLogError("ofxRakNet") << "couldn't connect to " << host << ":" << port << " (SLikeNet ConnectionAttemptResult " << result << ")";
		return false;
	}
	return true;
}

void Peer::disconnect(PeerId id) {
	if (peer) {
		peer->CloseConnection(SLNet::RakNetGUID(id), true);
	}
}

void Peer::stop() {
	if (peer) {
		peer->Shutdown(300);
		SLNet::RakPeerInterface::DestroyInstance(peer);
		peer = nullptr;
	}
	server = false;
}

bool Peer::isRunning() const {
	return peer && peer->IsActive();
}

uint16_t Peer::getPort() const {
	return peer ? peer->GetMyBoundAddress().GetPort() : 0;
}

bool Peer::sendRaw(PeerId target, bool broadcast, uint8_t type, const void * data, size_t size,
	Reliability reliability, Priority priority, uint8_t channel) {
	if (!peer) {
		ofLogWarning("ofxRakNet") << "send before startServer() / startClient()";
		return false;
	}
	if (type > MAX_MESSAGE_TYPE) {
		ofLogError("ofxRakNet") << "message type " << int(type) << " is above MAX_MESSAGE_TYPE (" << int(MAX_MESSAGE_TYPE) << ")";
		return false;
	}
	if (size + 1 > static_cast<size_t>(INT32_MAX)) {
		ofLogError("ofxRakNet") << "message too large: " << size << " bytes";
		return false;
	}
	// the type byte stays readable; the payload is sealed when encrypting
	std::string sealed;
	if (cipher.isEnabled()) {
		sealed = cipher.encrypt(std::string(static_cast<const char *>(data), size));
		if (sealed.empty()) {
			return false;
		}
		data = sealed.data();
		size = sealed.size();
	}
	sendBuffer.resize(size + 1);
	sendBuffer[0] = static_cast<char>(ID_USER_PACKET_ENUM + type);
	if (size > 0) {
		std::memcpy(sendBuffer.data() + 1, data, size);
	}
	SLNet::AddressOrGUID to = target ? SLNet::AddressOrGUID(SLNet::RakNetGUID(target)) : SLNet::AddressOrGUID(SLNet::UNASSIGNED_SYSTEM_ADDRESS);
	return peer->Send(sendBuffer.data(), static_cast<int>(sendBuffer.size()), toRakNet(priority), toRakNet(reliability),
		static_cast<char>(channel), to, broadcast) != 0;
}

bool Peer::send(PeerId id, uint8_t type, const void * data, size_t size, Reliability r, Priority p, uint8_t channel) {
	if (id == 0) {
		ofLogWarning("ofxRakNet") << "send to peer 0; use broadcast()";
		return false;
	}
	return sendRaw(id, false, type, data, size, r, p, channel);
}

bool Peer::send(PeerId id, uint8_t type, const std::string & text, Reliability r, Priority p, uint8_t channel) {
	return send(id, type, text.data(), text.size(), r, p, channel);
}

bool Peer::send(PeerId id, uint8_t type, const ofBuffer & buffer, Reliability r, Priority p, uint8_t channel) {
	return send(id, type, buffer.getData(), buffer.size(), r, p, channel);
}

bool Peer::send(PeerId id, uint8_t type, const ofJson & json, Reliability r, Priority p, uint8_t channel) {
	return send(id, type, json.dump(), r, p, channel);
}

bool Peer::broadcast(uint8_t type, const void * data, size_t size, PeerId except, Reliability r, Priority p, uint8_t channel) {
	// with broadcast=true RakNet sends to everyone except the given target
	return sendRaw(except, true, type, data, size, r, p, channel);
}

bool Peer::broadcast(uint8_t type, const std::string & text, PeerId except, Reliability r, Priority p, uint8_t channel) {
	return broadcast(type, text.data(), text.size(), except, r, p, channel);
}

bool Peer::broadcast(uint8_t type, const ofJson & json, PeerId except, Reliability r, Priority p, uint8_t channel) {
	return broadcast(type, json.dump(), except, r, p, channel);
}

void Peer::update() {
	if (!peer) {
		return;
	}
	for (auto * packet = peer->Receive(); packet; peer->DeallocatePacket(packet), packet = peer->Receive()) {
		if (packet->length == 0) {
			continue;
		}
		const auto id = packet->data[0];
		const PeerId from = packet->guid.g;
		const auto address = addressString(packet->systemAddress);

		if (id == ID_NEW_INCOMING_CONNECTION || id == ID_CONNECTION_REQUEST_ACCEPTED) {
			ConnectionEvent e { from, address };
			ofNotifyEvent(onConnect, e, this);
			continue;
		}

		DisconnectionEvent::Reason reason;
		bool disconnected = true;
		switch (id) {
		case ID_DISCONNECTION_NOTIFICATION: reason = DisconnectionEvent::Reason::Disconnected; break;
		case ID_CONNECTION_LOST: reason = DisconnectionEvent::Reason::Lost; break;
		case ID_CONNECTION_ATTEMPT_FAILED: reason = DisconnectionEvent::Reason::ConnectFailed; break;
		case ID_NO_FREE_INCOMING_CONNECTIONS: reason = DisconnectionEvent::Reason::ServerFull; break;
		case ID_INVALID_PASSWORD: reason = DisconnectionEvent::Reason::InvalidPassword; break;
		case ID_CONNECTION_BANNED: reason = DisconnectionEvent::Reason::Banned; break;
		case ID_INCOMPATIBLE_PROTOCOL_VERSION: reason = DisconnectionEvent::Reason::IncompatibleProtocol; break;
		case ID_ALREADY_CONNECTED: reason = DisconnectionEvent::Reason::AlreadyConnected; break;
		default: disconnected = false; reason = DisconnectionEvent::Reason::Disconnected; break;
		}
		if (disconnected) {
			DisconnectionEvent e { from, address, reason };
			ofNotifyEvent(onDisconnect, e, this);
			continue;
		}

		if (id >= ID_USER_PACKET_ENUM) {
			MessageEvent e;
			e.peer = from;
			e.address = address;
			e.type = static_cast<uint8_t>(id - ID_USER_PACKET_ENUM);
			std::string payload(reinterpret_cast<const char *>(packet->data + 1), packet->length - 1);
			e.encrypted = crypto::Cipher::isEnvelope(payload);
			if (e.encrypted) {
				auto plain = cipher.decrypt(payload);
				e.authentic = plain.has_value();
				if (plain) e.data.set(plain->data(), plain->size());
			} else {
				e.data.set(payload.data(), payload.size());
			}
			if (cipher.isEnabled() && requireEncryption && !e.authentic) {
				continue;
			}
			ofNotifyEvent(onMessage, e, this);
			continue;
		}

		ofNotifyEvent(onPacket, packet, this);
	}
}

void Peer::setPassphrase(const std::string & passphrase) {
	cipher.setPassphrase(passphrase);
}

void Peer::setAutoUpdate(bool autoUpdate) {
	if (autoUpdate) {
		updateListener = ofEvents().update.newListener([this](ofEventArgs &) { update(); }, OF_EVENT_ORDER_BEFORE_APP);
	} else {
		updateListener.unsubscribe();
	}
}

std::vector<PeerId> Peer::getConnections() const {
	std::vector<PeerId> result;
	if (!peer) {
		return result;
	}
	DataStructures::List<SLNet::SystemAddress> addresses;
	DataStructures::List<SLNet::RakNetGUID> guids;
	peer->GetSystemList(addresses, guids);
	for (unsigned int i = 0; i < guids.Size(); i++) {
		result.push_back(guids[i].g);
	}
	return result;
}

size_t Peer::getNumConnections() const {
	return peer ? peer->NumberOfConnections() : 0;
}

std::string Peer::getAddress(PeerId id) const {
	if (!peer) {
		return "";
	}
	auto address = peer->GetSystemAddressFromGuid(SLNet::RakNetGUID(id));
	return address == SLNet::UNASSIGNED_SYSTEM_ADDRESS ? "" : addressString(address);
}

int Peer::getPing(PeerId id) const {
	return peer ? peer->GetAveragePing(SLNet::RakNetGUID(id)) : -1;
}

PeerId Peer::getMyId() const {
	return peer ? peer->GetMyGUID().g : 0;
}

} // namespace ofxRakNet
