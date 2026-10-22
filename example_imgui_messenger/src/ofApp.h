#pragma once

#include "ofMain.h"
#include "ofxRakNet.h"
#include "ofxImGui.h"

// Sending and receiving with ofxRakNet, driven from Dear ImGui (needs ofxImGui,
// develop branch). Run two copies: the first starts a server on the port in
// bin/data/settings.json, the next ones connect to it (or use the Connection panel).
//
// - Connection: server / client, host, port, password, start / stop
// - Peers: everyone connected, with ping; pick who to send to; kick (server)
// - Compose: text or JSON, reliability, priority, channel; Send or Broadcast
// - Log: everything sent and received, with filters
// - Relay: a server can pass each client's messages on to the other clients.
//   Relayed messages carry the original sender's id in front of the payload.
class ofApp : public ofBaseApp {
public:
	void setup() override;
	void update() override;
	void draw() override;
	void exit() override;

	// message types (0 .. ofxRakNet::Peer::MAX_MESSAGE_TYPE)
	enum Type : uint8_t {
		TEXT = 0,       // UTF-8 text
		JSON = 1,       // JSON text
		RELAY_TEXT = 2, // 8 byte original sender id + TEXT payload (server -> clients)
		RELAY_JSON = 3, // 8 byte original sender id + JSON payload
	};

	struct LogEntry {
		float time;
		bool sent;
		ofxRakNet::PeerId peer;   // to / from (0: everyone)
		ofxRakNet::PeerId origin; // original sender of a relayed message (0: direct)
		uint8_t type;
		std::string reliability;
		size_t bytes;
		std::string text;
	};

	// networking
	void startServer();
	void startClient();
	void stop();
	void send(bool toEveryone);
	void sendTo(ofxRakNet::PeerId peer, uint8_t type, const std::string & payload, bool toEveryone, ofxRakNet::PeerId except = 0);
	void onConnect(ofxRakNet::ConnectionEvent & e);
	void onDisconnect(ofxRakNet::DisconnectionEvent & e);
	void onMessage(ofxRakNet::MessageEvent & e);
	void log(bool sent, ofxRakNet::PeerId peer, uint8_t type, const std::string & payload, ofxRakNet::PeerId origin = 0);
	void info(const std::string & line);
	std::string shortId(ofxRakNet::PeerId id) const;

	// panels
	void drawConnection();
	void drawPeers();
	void drawCompose();
	void drawLog();

	ofxRakNet::Peer peer;
	ofEventListeners listeners;
	ofxImGui::Gui gui;

	// connection settings
	int mode = 0; // 0 server, 1 client
	char host[128] = "127.0.0.1";
	int port = 60000;
	char password[64] = "";
	int maxConnections = 32;
	bool relay = true;
	bool greet = true;
	std::string status = "stopped";

	// compose
	int format = 0; // 0 text, 1 JSON
	int reliability = 3; // ReliableOrdered
	int priority = 1;    // High
	int channel = 0;
	char message[2048] = "hello from openFrameworks";
	ofxRakNet::PeerId target = 0; // 0: everyone

	// log
	std::deque<LogEntry> entries;
	std::deque<std::string> infos;
	bool showSent = true;
	bool showReceived = true;
	bool autoScroll = true;
	char filter[64] = "";
	size_t sentCount = 0, receivedCount = 0, sentBytes = 0, receivedBytes = 0;
};
