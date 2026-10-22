#pragma once

#include "ofMain.h"
#include "ofAppNoWindow.h"
#include "ofxUnitTests.h"
#include "ofxRakNet.h"

// SLikeNet internals, for the split-packet patch test
#include "slikenet/ReliabilityLayer.h"
#include "slikenet/InternalPacket.h"
// OpenSSL parts (openFrameworks' OpenSSL)
#include "slikenet/crypto/cryptomanager.h"
#include "slikenet/TCPInterface.h"

static_assert(OPEN_SSL_CLIENT_SUPPORT == 1, "TCPInterface should be built with TLS (defineoverrides.h)");

#include <chrono>
#include <thread>

using namespace ofxRakNet;

namespace detail {

// update the peers until done() or the timeout
template <typename Done>
bool pump(std::initializer_list<Peer *> peers, Done done, int timeoutMs = 5000) {
	auto start = ofGetElapsedTimeMillis();
	while (!done()) {
		if (ofGetElapsedTimeMillis() - start > uint64_t(timeoutMs)) return false;
		for (auto * p : peers) p->update();
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	return true;
}

// records a peer's events
struct Log {
	std::vector<ConnectionEvent> connects;
	std::vector<DisconnectionEvent> disconnects;
	std::vector<MessageEvent> messages;
	ofEventListeners listeners;
	void attach(Peer & peer) {
		listeners.push(peer.onConnect.newListener([this](ConnectionEvent & e) { connects.push_back(e); }));
		listeners.push(peer.onDisconnect.newListener([this](DisconnectionEvent & e) { disconnects.push_back(e); }));
		listeners.push(peer.onMessage.newListener([this](MessageEvent & e) { messages.push_back(e); }));
	}
};

} // namespace

class ofApp : public ofxUnitTestsApp {
	void run() {
		testConnectAndMessages();
		testBroadcast();
		testPassword();
		testServerFull();
		testSplitPacketPatch();
		testCrypto();
	}

	void testConnectAndMessages() {
		Peer server, client;
		detail::Log s, c;
		s.attach(server);
		c.attach(client);
		ofxTest(server.startServer(0, 8), "startServer");
		ofxTest(server.isRunning() && server.isServer(), "server running");
		uint16_t port = server.getPort();
		ofxTest(port != 0, "server bound to port " + ofToString(port));
		ofxTest(client.connect("127.0.0.1", port), "connect");
		ofxTest(detail::pump({ &server, &client }, [&] { return !s.connects.empty() && !c.connects.empty(); }), "both sides connect");
		if (s.connects.empty() || c.connects.empty()) return;

		auto serverId = c.connects[0].peer;
		auto clientId = s.connects[0].peer;
		ofxTest(serverId == server.getMyId(), "client sees the server's id");
		ofxTest(clientId == client.getMyId(), "server sees the client's id");
		ofxTestEq(server.getNumConnections(), size_t(1), "one connection");
		ofxTest(!server.getAddress(clientId).empty(), "getAddress");

		ofxTest(client.send(serverId, 3, std::string("hello")), "send text");
		ofxTest(detail::pump({ &server, &client }, [&] { return !s.messages.empty(); }), "server gets the text");
		if (!s.messages.empty()) {
			ofxTestEq(int(s.messages[0].type), 3, "message type");
			ofxTestEq(s.messages[0].getText(), std::string("hello"), "message text");
			ofxTest(s.messages[0].peer == clientId, "message from the client");
		}

		ofJson json = { { "x", 1.5 }, { "name", "of" }, { "list", { 1, 2, 3 } } };
		ofxTest(server.send(clientId, 7, json), "send json");
		ofxTest(detail::pump({ &server, &client }, [&] { return !c.messages.empty(); }), "client gets the json");
		if (!c.messages.empty()) {
			ofxTest(c.messages[0].getJson() == json, "json round trip");
		}

		// 1 MB is split into ~750 fragments and reassembled (the patched path)
		std::string big(1024 * 1024, '\0');
		for (size_t i = 0; i < big.size(); i++) big[i] = char((i * 2654435761u) >> 13);
		ofxTest(client.send(serverId, 9, ofBuffer(big.data(), big.size())), "send 1 MB");
		ofxTest(detail::pump({ &server, &client }, [&] { return s.messages.size() >= 2; }, 15000), "server gets 1 MB");
		if (s.messages.size() >= 2) {
			ofxTestEq(s.messages[1].data.size(), big.size(), "1 MB size");
			ofxTest(s.messages[1].data.getText() == big, "1 MB contents intact");
		}

		ofxTest(!client.send(serverId, uint8_t(Peer::MAX_MESSAGE_TYPE + 1), std::string("x")), "type above MAX_MESSAGE_TYPE refused");

		client.disconnect(serverId);
		ofxTest(detail::pump({ &server, &client }, [&] { return !s.disconnects.empty(); }), "server sees the disconnect");
		if (!s.disconnects.empty()) {
			ofxTest(s.disconnects[0].reason == DisconnectionEvent::Reason::Disconnected, "reason: disconnected");
		}
		server.stop();
		ofxTest(!server.isRunning(), "stop");
	}

	void testBroadcast() {
		Peer server, a, b;
		detail::Log s, la, lb;
		s.attach(server);
		la.attach(a);
		lb.attach(b);
		server.startServer(0, 8);
		a.connect("127.0.0.1", server.getPort());
		b.connect("127.0.0.1", server.getPort());
		ofxTest(detail::pump({ &server, &a, &b }, [&] { return s.connects.size() == 2; }), "two clients connect");
		if (s.connects.size() < 2) return;
		ofxTestEq(server.getConnections().size(), size_t(2), "getConnections");

		server.broadcast(1, std::string("all"));
		ofxTest(detail::pump({ &server, &a, &b }, [&] { return !la.messages.empty() && !lb.messages.empty(); }), "broadcast reaches both");

		auto aId = a.getMyId();
		server.broadcast(2, std::string("not a"), aId);
		ofxTest(detail::pump({ &server, &a, &b }, [&] { return lb.messages.size() == 2; }), "broadcast with except reaches b");
		detail::pump({ &server, &a, &b }, [] { return false; }, 200);
		ofxTestEq(la.messages.size(), size_t(1), "broadcast with except skips a");
	}

	void testPassword() {
		Peer server, client;
		detail::Log c;
		c.attach(client);
		server.startServer(0, 8, "secret");
		client.connect("127.0.0.1", server.getPort(), "wrong");
		ofxTest(detail::pump({ &server, &client }, [&] { return !c.disconnects.empty(); }), "wrong password refused");
		if (!c.disconnects.empty()) {
			ofxTest(c.disconnects[0].reason == DisconnectionEvent::Reason::InvalidPassword, "reason: " + toString(c.disconnects[0].reason));
		}

		Peer good;
		detail::Log g;
		g.attach(good);
		good.connect("127.0.0.1", server.getPort(), "secret");
		ofxTest(detail::pump({ &server, &good }, [&] { return !g.connects.empty(); }), "right password connects");
	}

	void testServerFull() {
		Peer server, first, second;
		detail::Log f, sec;
		f.attach(first);
		sec.attach(second);
		server.startServer(0, 1);
		first.connect("127.0.0.1", server.getPort());
		ofxTest(detail::pump({ &server, &first }, [&] { return !f.connects.empty(); }), "first client connects");
		second.connect("127.0.0.1", server.getPort());
		ofxTest(detail::pump({ &server, &first, &second }, [&] { return !sec.disconnects.empty(); }), "second client refused");
		if (!sec.disconnects.empty()) {
			ofxTest(sec.disconnects[0].reason == DisconnectionEvent::Reason::ServerFull, "reason: " + toString(sec.disconnects[0].reason));
		}
	}

	// SLikeNet's crypto helpers, ported to OpenSSL 1.1+ (patch 0003)
	void testCrypto() {
		using SLNet::Experimental::Crypto::CCryptoManager;
		ofxTest(CCryptoManager::Initialize(), "CCryptoManager::Initialize (OpenSSL)");
		const std::string plain = "openFrameworks + SLikeNet + OpenSSL " OPENSSL_VERSION_TEXT;
		size_t encryptedSize = plain.size();
		ofxTest(CCryptoManager::GetRequiredEncryptionBufferSize(encryptedSize), "encryption buffer size");
		std::vector<unsigned char> encrypted(encryptedSize);
		ofxTest(CCryptoManager::EncryptSessionData(reinterpret_cast<const unsigned char *>(plain.data()), plain.size(),
			encrypted.data(), encryptedSize), "AES-256 encrypt");
		ofxTest(encryptedSize > 0 && std::memcmp(encrypted.data(), plain.data(), std::min(plain.size(), encryptedSize)) != 0, "ciphertext differs");
		std::vector<unsigned char> decrypted(encryptedSize);
		size_t decryptedSize = decrypted.size();
		ofxTest(CCryptoManager::DecryptSessionData(encrypted.data(), encryptedSize, decrypted.data(), decryptedSize), "AES-256 decrypt");
		ofxTest(std::string(reinterpret_cast<const char *>(decrypted.data()), decryptedSize) == plain, "round trip");
		ofLogNotice() << "OpenSSL: " << OPENSSL_VERSION_TEXT;
	}

	// SLikeNet issue #82, defect 1: fragments that don't match the array allocated
	// for their message must be rejected (the original only had RakAssert, empty in release)
	void testSplitPacketPatch() {
		SLNet::InternalPacket first;
		first.splitPacketId = 7;
		first.splitPacketCount = 4;
		first.splitPacketIndex = 0;
		SLNet::SplitPacketSort sort;
		sort.Preallocate(&first, __FILE__, __LINE__);
		ofxTest(sort.Add(&first), "fragment 0 accepted");
		ofxTest(!sort.Add(&first), "duplicate fragment rejected");

		SLNet::InternalPacket outOfRange = first;
		outOfRange.splitPacketIndex = 4;
		ofxTest(!sort.Add(&outOfRange), "index == count rejected");

		SLNet::InternalPacket otherCount = first;
		otherCount.splitPacketCount = 100000;
		otherCount.splitPacketIndex = 50000;
		ofxTest(!sort.Add(&otherCount), "fragment declaring a larger count rejected (was an out-of-bounds write)");

		SLNet::InternalPacket otherId = first;
		otherId.splitPacketId = 8;
		otherId.splitPacketIndex = 1;
		ofxTest(!sort.Add(&otherId), "fragment of another message rejected");

		SLNet::InternalPacket second = first;
		second.splitPacketIndex = 1;
		ofxTest(sort.Add(&second), "matching fragment 1 accepted");
		ofxTestEq(sort.GetNumAddedPackets(), 2u, "two fragments added");
	}
};

