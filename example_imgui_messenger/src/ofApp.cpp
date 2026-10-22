#include "ofApp.h"

using namespace ofxRakNet;

namespace {

const char * RELIABILITY_NAMES[] = { "Unreliable", "UnreliableSequenced", "Reliable", "ReliableOrdered", "ReliableSequenced" };
const char * PRIORITY_NAMES[] = { "Immediate", "High", "Medium", "Low" };
const char * TYPE_NAMES[] = { "text", "json", "text (relay)", "json (relay)" };

const ImVec4 SENT_COLOR(0.55f, 0.8f, 1.f, 1.f);
const ImVec4 RECEIVED_COLOR(0.6f, 1.f, 0.6f, 1.f);
const ImVec4 DIM_COLOR(0.6f, 0.6f, 0.6f, 1.f);

std::string payloadText(const ofBuffer & data, size_t offset = 0) {
	if (data.size() <= offset) return "";
	return std::string(data.getData() + offset, data.size() - offset);
}

} // namespace

//--------------------------------------------------------------
void ofApp::setup() {
	ofBackground(24);
	ofSetFrameRate(60);
	gui.setup(nullptr, true);

	auto settings = ofLoadJson("settings.json");
	std::snprintf(host, sizeof(host), "%s", settings.value("host", std::string("127.0.0.1")).c_str());
	port = settings.value("port", 60000);

	listeners.push(peer.onConnect.newListener(this, &ofApp::onConnect));
	listeners.push(peer.onDisconnect.newListener(this, &ofApp::onDisconnect));
	listeners.push(peer.onMessage.newListener(this, &ofApp::onMessage));

	// first copy: server; the port is taken for the next copies, so they connect
	startServer();
	if (!peer.isRunning()) {
		mode = 1;
		startClient();
	}
}

void ofApp::exit() {
	stop();
}

//--------------------------------------------------------------
void ofApp::startServer() {
	stop();
	if (peer.startServer(uint16_t(port), unsigned(maxConnections), password)) {
		status = "server on port " + ofToString(peer.getPort());
		info(status);
	} else {
		status = "couldn't listen on port " + ofToString(port);
	}
}

void ofApp::startClient() {
	stop();
	if (peer.connect(host, uint16_t(port), password)) {
		status = "connecting to " + std::string(host) + ":" + ofToString(port);
		info(status);
	} else {
		status = "couldn't connect";
	}
}

void ofApp::stop() {
	if (peer.isRunning()) {
		peer.stop();
		info("stopped");
	}
	status = "stopped";
	target = 0;
}

std::string ofApp::shortId(PeerId id) const {
	if (id == 0) return "everyone";
	auto hex = ofToHex(id);
	return hex.substr(hex.size() - 6) + (id == peer.getMyId() ? " (me)" : "");
}

void ofApp::info(const std::string & line) {
	infos.push_back(ofGetTimestampString("%H:%M:%S") + "  " + line);
	while (infos.size() > 200) infos.pop_front();
}

void ofApp::log(bool sent, PeerId to, uint8_t type, const std::string & payload, PeerId origin) {
	entries.push_back({ ofGetElapsedTimef(), sent, to, origin, type,
		RELIABILITY_NAMES[sent ? reliability : 3], payload.size(), payload });
	while (entries.size() > 1000) entries.pop_front();
	if (sent) {
		sentCount++;
		sentBytes += payload.size();
	} else {
		receivedCount++;
		receivedBytes += payload.size();
	}
}

//--------------------------------------------------------------
void ofApp::sendTo(PeerId to, uint8_t type, const std::string & payload, bool toEveryone, PeerId except) {
	auto r = static_cast<Reliability>(reliability);
	auto p = static_cast<Priority>(priority);
	if (toEveryone) {
		peer.broadcast(type, payload.data(), payload.size(), except, r, p, uint8_t(channel));
	} else {
		peer.send(to, type, payload.data(), payload.size(), r, p, uint8_t(channel));
	}
}

void ofApp::send(bool toEveryone) {
	std::string payload = message;
	if (payload.empty()) return;
	uint8_t type = TEXT;
	if (format == 1) {
		auto json = ofJson::parse(payload, nullptr, false);
		if (json.is_discarded()) {
			info("not valid JSON, not sent");
			return;
		}
		payload = json.dump();
		type = JSON;
	}
	toEveryone = toEveryone || target == 0;
	sendTo(target, type, payload, toEveryone);
	log(true, toEveryone ? 0 : target, type, payload);
}

//--------------------------------------------------------------
void ofApp::onConnect(ConnectionEvent & e) {
	info("connected: " + shortId(e.peer) + "  " + e.address);
	if (!peer.isServer()) {
		status = "connected to " + e.address;
	}
	if (greet) {
		auto me = ofToHex(peer.getMyId());
		ofJson hello = { { "hello", me.substr(me.size() - 6) }, { "role", peer.isServer() ? "server" : "client" },
			{ "of", ofGetVersionInfo() } };
		auto payload = hello.dump();
		peer.send(e.peer, JSON, payload);
		entries.push_back({ ofGetElapsedTimef(), true, e.peer, 0, JSON, "ReliableOrdered", payload.size(), payload });
		sentCount++;
		sentBytes += payload.size();
	}
}

void ofApp::onDisconnect(DisconnectionEvent & e) {
	info(shortId(e.peer) + "  " + e.address + ": " + toString(e.reason));
	if (target == e.peer) target = 0;
	if (!peer.isServer()) {
		status = toString(e.reason);
	}
}

void ofApp::onMessage(MessageEvent & e) {
	if (e.type == TEXT || e.type == JSON) {
		auto payload = payloadText(e.data);
		log(false, e.peer, e.type, payload);
		// the server passes it on, with the original sender in front
		if (peer.isServer() && relay && peer.getNumConnections() > 1) {
			std::string relayed(sizeof(PeerId), '\0');
			std::memcpy(relayed.data(), &e.peer, sizeof(PeerId));
			relayed += payload;
			peer.broadcast(e.type == TEXT ? RELAY_TEXT : RELAY_JSON, relayed.data(), relayed.size(), e.peer);
		}
	} else if ((e.type == RELAY_TEXT || e.type == RELAY_JSON) && e.data.size() >= sizeof(PeerId)) {
		PeerId origin;
		std::memcpy(&origin, e.data.getData(), sizeof(origin));
		log(false, e.peer, e.type, payloadText(e.data, sizeof(PeerId)), origin);
	}
}

//--------------------------------------------------------------
void ofApp::update() {
	peer.update();
}

void ofApp::draw() {
	gui.begin();
	ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(ofGetWidth() - 20, ofGetHeight() - 20), ImGuiCond_FirstUseEver);
	ImGui::Begin("ofxRakNet messenger");
	if (ImGui::BeginTable("layout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
		ImGui::TableSetupColumn("left", ImGuiTableColumnFlags_WidthStretch, 0.38f);
		ImGui::TableSetupColumn("right", ImGuiTableColumnFlags_WidthStretch, 0.62f);
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		drawConnection();
		drawPeers();
		ImGui::TableNextColumn();
		drawCompose();
		drawLog();
		ImGui::EndTable();
	}
	ImGui::End();
	gui.end();
}

//--------------------------------------------------------------
void ofApp::drawConnection() {
	ImGui::SeparatorText("Connection");
	bool running = peer.isRunning();
	ImGui::BeginDisabled(running);
	ImGui::RadioButton("server", &mode, 0);
	ImGui::SameLine();
	ImGui::RadioButton("client", &mode, 1);
	if (mode == 1) ImGui::InputText("host", host, sizeof(host));
	ImGui::InputInt("port", &port);
	port = ofClamp(port, 1, 65535);
	ImGui::InputText("password", password, sizeof(password), ImGuiInputTextFlags_Password);
	if (mode == 0) {
		ImGui::SliderInt("max connections", &maxConnections, 1, 64);
	}
	ImGui::EndDisabled();

	if (!running) {
		if (ImGui::Button(mode == 0 ? "Start server" : "Connect", ImVec2(-FLT_MIN, 0))) {
			mode == 0 ? startServer() : startClient();
		}
	} else if (ImGui::Button("Stop", ImVec2(-FLT_MIN, 0))) {
		stop();
	}
	ImGui::TextColored(running ? RECEIVED_COLOR : DIM_COLOR, "%s", status.c_str());
	if (running) {
		ImGui::TextDisabled("me: %s  port %d", shortId(peer.getMyId()).c_str(), int(peer.getPort()));
	}
	ImGui::Checkbox("relay to other clients", &relay);
	ImGui::SetItemTooltip("server: pass each client's messages on to the others");
	ImGui::SameLine();
	ImGui::Checkbox("greet", &greet);
	ImGui::SetItemTooltip("send a JSON hello when a connection opens");
}

void ofApp::drawPeers() {
	ImGui::SeparatorText("Peers");
	auto connections = peer.getConnections();
	ImGui::Text("%zu connected", connections.size());
	if (ImGui::BeginTable("peers", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY,
			ImVec2(0, 180))) {
		ImGui::TableSetupColumn("peer");
		ImGui::TableSetupColumn("address");
		ImGui::TableSetupColumn("ping", ImGuiTableColumnFlags_WidthFixed, 40);
		ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 40);
		ImGui::TableHeadersRow();

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		if (ImGui::Selectable("everyone", target == 0, ImGuiSelectableFlags_SpanAllColumns)) target = 0;
		for (auto id : connections) {
			ImGui::PushID(int(id & 0x7fffffff));
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			if (ImGui::Selectable(shortId(id).c_str(), target == id, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
				target = id;
			}
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(peer.getAddress(id).c_str());
			ImGui::TableNextColumn();
			ImGui::Text("%d", peer.getPing(id));
			ImGui::TableNextColumn();
			if (peer.isServer() && ImGui::SmallButton("kick")) {
				peer.disconnect(id);
			}
			ImGui::PopID();
		}
		ImGui::EndTable();
	}

	ImGui::SeparatorText("Events");
	if (ImGui::BeginChild("events", ImVec2(0, 0), ImGuiChildFlags_Borders)) {
		for (auto & line : infos) ImGui::TextUnformatted(line.c_str());
		if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.f);
	}
	ImGui::EndChild();
}

void ofApp::drawCompose() {
	ImGui::SeparatorText("Compose");
	ImGui::Text("to: %s", shortId(target).c_str());
	ImGui::SameLine();
	ImGui::TextDisabled("(pick in Peers)");

	ImGui::RadioButton("text", &format, 0);
	ImGui::SameLine();
	ImGui::RadioButton("JSON", &format, 1);
	ImGui::SameLine();
	if (format == 1) {
		bool valid = !ofJson::parse(std::string(message), nullptr, false).is_discarded();
		ImGui::TextColored(valid ? RECEIVED_COLOR : ImVec4(1, 0.4f, 0.4f, 1), valid ? "valid" : "invalid JSON");
		ImGui::SameLine();
		if (ImGui::SmallButton("format") && valid) {
			std::snprintf(message, sizeof(message), "%s", ofJson::parse(std::string(message)).dump(2).c_str());
		}
	}

	ImGui::SetNextItemWidth(170);
	ImGui::Combo("reliability", &reliability, RELIABILITY_NAMES, IM_ARRAYSIZE(RELIABILITY_NAMES));
	ImGui::SameLine();
	ImGui::SetNextItemWidth(100);
	ImGui::Combo("priority", &priority, PRIORITY_NAMES, IM_ARRAYSIZE(PRIORITY_NAMES));
	ImGui::SameLine();
	ImGui::SetNextItemWidth(70);
	ImGui::InputInt("channel", &channel);
	channel = ofClamp(channel, 0, 31);

	bool enter = false;
	if (format == 0) {
		ImGui::SetNextItemWidth(-FLT_MIN);
		enter = ImGui::InputText("##message", message, sizeof(message), ImGuiInputTextFlags_EnterReturnsTrue);
	} else {
		ImGui::InputTextMultiline("##json", message, sizeof(message), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 5));
	}
	ImGui::BeginDisabled(!peer.isRunning() || peer.getNumConnections() == 0);
	if (ImGui::Button(target == 0 ? "Send to everyone" : "Send to peer") || enter) {
		send(false);
		if (enter && format == 0) {
			message[0] = 0;
			ImGui::SetKeyboardFocusHere(-1);
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Broadcast")) send(true);
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::TextDisabled("sent %zu (%zu B)  received %zu (%zu B)", sentCount, sentBytes, receivedCount, receivedBytes);
}

void ofApp::drawLog() {
	ImGui::SeparatorText("Log");
	ImGui::Checkbox("sent", &showSent);
	ImGui::SameLine();
	ImGui::Checkbox("received", &showReceived);
	ImGui::SameLine();
	ImGui::Checkbox("auto-scroll", &autoScroll);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(160);
	ImGui::InputTextWithHint("##filter", "filter", filter, sizeof(filter));
	ImGui::SameLine();
	if (ImGui::Button("Clear")) {
		entries.clear();
		sentCount = receivedCount = sentBytes = receivedBytes = 0;
	}

	if (ImGui::BeginTable("log", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable
			| ImGuiTableFlags_BordersInnerV, ImVec2(0, -FLT_MIN))) {
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("time", ImGuiTableColumnFlags_WidthFixed, 55);
		ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 18);
		ImGui::TableSetupColumn("peer", ImGuiTableColumnFlags_WidthFixed, 130);
		ImGui::TableSetupColumn("type", ImGuiTableColumnFlags_WidthFixed, 90);
		ImGui::TableSetupColumn("bytes", ImGuiTableColumnFlags_WidthFixed, 45);
		ImGui::TableSetupColumn("message", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableHeadersRow();
		const std::string needle = ofToLower(filter);
		for (auto & e : entries) {
			if ((e.sent && !showSent) || (!e.sent && !showReceived)) continue;
			if (!needle.empty() && ofToLower(e.text).find(needle) == std::string::npos) continue;
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextDisabled("%.1f", e.time);
			ImGui::TableNextColumn();
			ImGui::TextColored(e.sent ? SENT_COLOR : RECEIVED_COLOR, e.sent ? "->" : "<-");
			ImGui::TableNextColumn();
			if (e.origin) {
				ImGui::Text("%s", shortId(e.origin).c_str());
				ImGui::SetItemTooltip("relayed by %s", shortId(e.peer).c_str());
			} else {
				ImGui::TextUnformatted(shortId(e.peer).c_str());
			}
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(TYPE_NAMES[std::min<int>(e.type, 3)]);
			if (e.sent) ImGui::SetItemTooltip("%s", e.reliability.c_str());
			ImGui::TableNextColumn();
			ImGui::Text("%zu", e.bytes);
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(e.text.c_str());
			if ((e.type == JSON || e.type == RELAY_JSON) && ImGui::IsItemHovered()) {
				auto json = ofJson::parse(e.text, nullptr, false);
				if (!json.is_discarded()) ImGui::SetTooltip("%s", json.dump(2).c_str());
			}
		}
		if (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.f);
		ImGui::EndTable();
	}
}
