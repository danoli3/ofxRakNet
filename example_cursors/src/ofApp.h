#pragma once

#include "ofMain.h"
#include "ofxRakNet.h"

// Run this example twice. The first copy becomes the server on the port in
// bin/data/settings.json; the next ones find the port taken and connect to it.
// Every copy sends its mouse position, the server relays them, and each window
// draws everyone's cursor.
class ofApp : public ofBaseApp {
public:
	void setup() override;
	void update() override;
	void draw() override;

	enum MessageType : uint8_t {
		CURSOR = 0, // Cursor payload
		LEAVE = 1,  // PeerId payload: that peer left (server -> clients)
	};

	struct Cursor {
		ofxRakNet::PeerId peer;
		float x, y;
	};

	void onMessage(ofxRakNet::MessageEvent & e);
	void onConnect(ofxRakNet::ConnectionEvent & e);
	void onDisconnect(ofxRakNet::DisconnectionEvent & e);

	ofxRakNet::Peer peer;
	ofEventListeners listeners;
	std::map<ofxRakNet::PeerId, glm::vec2> cursors;
	std::string status;
};
