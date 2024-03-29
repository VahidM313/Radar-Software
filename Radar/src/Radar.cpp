#include "Walnut/Application.h"
#include "Walnut/EntryPoint.h"
#include "Walnut/Image.h"
#include <imgui_internal.h>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <cmath>
#include <mosquitto.h>
#include <boost/json.hpp>

void embraceTheDarkness();
void on_connect(struct mosquitto* mosq, void* obj, int result);
void on_message(struct mosquitto* mosq, void* obj, const struct mosquitto_message* message);
void sendMQTTMessage(struct mosquitto* mosq, const std::string& direction);

struct TableEntry {
	unsigned long id;
	int distance;
	int degree;
	std::string time;
};

struct Target {
	int distance{};
	int degree{};
	bool isVisible{ true };
	bool isShow{ false };
	int count{};
};

int dist{}, deg{};
bool error{ false }, sweep{ false };

typedef std::vector<Target> vectar;

vectar targets{};

std::vector<std::pair<int, int>> vec;

std::vector<TableEntry> tableEntries;

class ExampleLayer : public Walnut::Layer {
public:
	virtual void OnUIRender() override {
		embraceTheDarkness();
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(17.0f / 255.0f, 17.0f / 255.0f, 17.0f / 255.0f, 1.0f));
		ImGui::Begin("setting");
		ImGui::Text("Control Buttons");
		ImGui::Spacing();
		if (ImGui::Button("Generate")) {
			GenerateRandomTargets(30);
		}
		if (ImGui::Button("Sweep")) {
			sweep = !sweep;
		}
		if (ImGui::Button("Left")) {
			sendMQTTMessage(mosq, "left");
		}
		ImGui::SameLine();
		if (ImGui::Button("Stop")) {
			sendMQTTMessage(mosq, "stop");
		}
		ImGui::SameLine();
		if (ImGui::Button("Right")) {
			sendMQTTMessage(mosq, "right");
		}
		if (error)
			ImGui::Text("unable to connect");
		ImGui::End();

		ImGui::Begin("Radar");
		RenderRadar();
		ImGui::End();

		ImGui::Begin("Targets");
		RenderTargetTable();
		ImGui::End();

		ImGui::Begin("Distance");
		RenderValue(dist, "cm");
		ImGui::End();

		ImGui::Begin("Degree");
		RenderValue(deg, "");
		ImGui::End();

		ImGui::PopStyleColor();
	}

	ExampleLayer() {
		for (int i = 0; i <= 360; ++i) {
			vec.push_back(std::make_pair(i, 0));
		}
		// Initialize the Mosquitto library
		mosquitto_lib_init();

		// Create a new Mosquitto client
		mosq = mosquitto_new(NULL, true, NULL);
		if (!mosq) {
			//std::cerr << "Error: Out of memory.\n";
			return;
		}

		// Set up the necessary callbacks
		mosquitto_connect_callback_set(mosq, on_connect);
		mosquitto_message_callback_set(mosq, on_message);

		// Connect to the Mosquitto broker
		if (mosquitto_connect(mosq, "localhost", 1883, 60)) {
			//std::cerr << "Unable to connect.\n";
			error = true;
			return;
		}

		// Start the main network loop
		mosquitto_loop_start(mosq);
	}

	~ExampleLayer() {
		// Clean up the Mosquitto client and library
		mosquitto_destroy(mosq);
		mosquitto_lib_cleanup();
	}


private:
	int counter{};
	struct mosquitto* mosq;
	void GenerateRandomTargets(int count) {
		for (int i = 0; i < count; i++) {
			Target target;
			target.distance = std::rand() % 360; // Random distance between 0 and 500
			target.degree = std::rand() % 360; // Random degree between 0 and 360
			targets.push_back(target);
		}
	}

	std::string GetCurrentTime() const {
		auto now = std::chrono::system_clock::now();
		auto now_c = std::chrono::system_clock::to_time_t(now);
		std::tm* now_tm = std::localtime(&now_c);
		char time_str[9];
		std::strftime(time_str, sizeof(time_str), "%H:%M:%S", now_tm);
		return std::string(time_str);
	}

	void RenderValue(int value, const char* notation) {
		ImVec2 frameSize = ImGui::GetWindowSize();
		ImVec2 centerPos(frameSize.x * 0.5f, frameSize.y * 0.5f);
		char valueStr[32];
		snprintf(valueStr, sizeof(valueStr), "%d", value);
		ImGui::SetWindowFontScale(3.0f);
		ImVec2 valueTextSize = ImGui::CalcTextSize(valueStr);
		ImVec2 valueTextPos(centerPos.x - valueTextSize.x * 0.5f, centerPos.y - valueTextSize.y * 0.5f);
		ImGui::SetCursorPos(valueTextPos);
		ImGui::Text("%s", valueStr);
		ImGui::SetWindowFontScale(1.5f);
		ImVec2 cmTextPos(centerPos.x + valueTextSize.x * 0.5f, centerPos.y + valueTextSize.y * 0.5f);
		ImGui::SetCursorPos(cmTextPos);
		ImGui::Text(notation);
	}

	void RenderTargetTable() {
		ImGui::Text("Detected Targets");
		ImGui::Columns(4, "Target Detected");
		ImGui::Separator();
		ImGui::Text("ID"); ImGui::NextColumn();
		ImGui::Text("Distance"); ImGui::NextColumn();
		ImGui::Text("Degree"); ImGui::NextColumn();
		ImGui::Text("Time"); ImGui::NextColumn();
		ImGui::Separator();
		int selected = -1;
		for (int i = tableEntries.size() - 1; i >= 0; i--) {
			char label[32];
			sprintf(label, "%04d", tableEntries[i].id);
			if (ImGui::Selectable(label, selected == i, ImGuiSelectableFlags_SpanAllColumns)) {
				selected = i;
			}
			ImGui::NextColumn();
			ImGui::Text("%d", tableEntries[i].distance); ImGui::NextColumn();
			ImGui::Text("%d", tableEntries[i].degree); ImGui::NextColumn();
			ImGui::Text("%s", tableEntries[i].time.c_str()); ImGui::NextColumn();
		}
		ImGui::Columns(1);
		ImGui::Separator();
	}

	void RenderRadar() {
		ImVec2 windowSize = ImGui::GetWindowSize();
		ImVec2 center = ImVec2(windowSize.x / 2.0f, windowSize.y / 2.0f + 30);
		float radius = (windowSize.y * 0.8f) / 2.0f;
		static float angle = 0.0f;

		if (sweep) {
			const float numLine{ 360.0f };
			const int fade{ 130 };
			for (float i{}; i < numLine; i += 0.1f) {
				float diff = angle - i;
				float radian = i * IM_PI / 180;
				ImVec2 end = ImVec2(center.x + cos(radian) * radius, center.y + sin(radian) * radius);
				int alpha{};
				if (diff > 0) {
					alpha = fade - (diff * fade) / 360.0f;
				}
				else {
					alpha = (-1 * diff * fade) / 360.0f;
				}
				ImGui::GetWindowDrawList()->AddLine(center, end, IM_COL32(5, 53, 4, alpha), 2.0f);
			}
			for (float i{}; i < numLine; i += 0.1f) {
				float diff = angle - i;
				float radian = i * IM_PI / 180;
				ImVec2 end = ImVec2(center.x + cos(radian) * radius, center.y + sin(radian) * radius);
				int alpha{};
				if (diff >= 0 && diff <= 1) {
					alpha = 255;
				}
				ImGui::GetWindowDrawList()->AddLine(center, end, IM_COL32(18, 182, 13, alpha), 2.0f);
			}
		}

		for (int i = 0; i < 360; i += 30) {
			RenderRadarLine(center, radius, angle, i, true);
		}

		for (int i = 0; i < 360; i += 5) {
			if (i % 30 == 0) {
				continue;
			}
			RenderRadarLine(center, radius, angle, i, false);
		}

		if (sweep) {
			for (auto& target : targets) {
				float targetRadian = target.degree * IM_PI / 180;
				float diff = angle - target.degree;
				if (diff < 0 && target.count == 1) {
					diff += 360;
				}
				diff = floor(diff * 10) / 10;
				if (diff > 0 && target.isShow) {
					int alpha = static_cast<int>(255 - 255 * (diff / 360.0f));
					RenderTargetLines(center, radius, target.degree, target.distance, alpha);
				}
				if (diff == 0)
				{
					target.count++;
					if (target.count) {
						AddTargetToTable(target);
						target.isShow = true;
					}
					if (target.count == 1) {
						dist = target.distance;
						deg = target.degree;
					}
					if (target.count == 2)
						target.isVisible = false;
				}
			}
		}
		else {
			for (auto& t : vec) {
				int alpha = static_cast<int>(255 - 255 * (t.second / 450.0f));
				RenderTargetLines(center, radius, t.first, t.second, alpha);
			}
		}

		RenderDistanceCircles(center, radius, 50, 450);

		ImGui::GetWindowDrawList()->AddCircleFilled(center, 5.0f, IM_COL32(255, 255, 255, 255, 255));

		targets.erase(std::remove_if(targets.begin(), targets.end(), [](const Target& target) {
			return !target.isVisible;
			}), targets.end());

		angle += 0.1f;
		if (angle > 360.0f) {
			angle = 0.0f;
		}
	}

	void RenderRadarLine(const ImVec2& center, float radius, float angle, int degree, bool isMainLine) {
		float radian = degree * IM_PI / 180;
		ImVec2 start = ImVec2(center.x + cos(radian) * radius, center.y + sin(radian) * radius);
		ImVec2 end = ImVec2(center.x + cos(radian) * (radius + (isMainLine ? 20 : 10)), center.y + sin(radian) * (radius + (isMainLine ? 20 : 10)));

		if (degree % 90 == 0) {
			ImGui::GetWindowDrawList()->AddLine(center, end, IM_COL32(150, 150, 150, 150), 3.0f);
		}
		else {
			ImGui::GetWindowDrawList()->AddLine(start, end, IM_COL32(255, 255, 255, 255), isMainLine ? 4.5f : 2.5f);
		}
		if (degree % 90 == 0) {
			ImGui::GetWindowDrawList()->AddLine(start, end, IM_COL32(255, 255, 255, 255), isMainLine ? 4.5f : 2.5f);
		}

		if (degree % 30 == 0) {
			ImVec2 textPos = ImVec2(end.x + 30 * cos(radian) - 10, end.y + 30 * sin(radian) - 8);
			ImGui::GetWindowDrawList()->AddText(textPos, IM_COL32(255, 255, 255, 255), std::to_string(degree).c_str());
		}
		else if (degree % 5 == 0) {
			ImGui::SetWindowFontScale(0.8f);
			ImVec2 textPos = ImVec2(end.x + 20 * cos(radian) - 15, end.y + 20 * sin(radian) - 6);
			ImGui::GetWindowDrawList()->AddText(textPos, IM_COL32(150, 150, 150, 150), std::to_string(degree).c_str());
			ImGui::SetWindowFontScale(1.0f);
		}
	}

	void RenderDistanceCircles(const ImVec2& center, float radius, int separate, int maxDistance) {
		const float distanceRate = radius * separate / maxDistance;
		const int x = radius / distanceRate;

		for (int i = 1; i <= x + 1; i++) {
			const float r = i * distanceRate;
			ImVec2 textPos = ImVec2(center.x + r + 2, center.y + 6);

			if (i == x + 1) {
				ImGui::GetWindowDrawList()->AddCircle(center, r, IM_COL32(255, 255, 255, 255), 0, 3.0f);
			}
			else {
				ImGui::GetWindowDrawList()->AddCircle(center, r, IM_COL32(225, 225, 225, 100), 0, 2.0f);
				ImGui::GetWindowDrawList()->AddText(textPos, IM_COL32(255, 255, 255, 200), std::to_string(i * separate).c_str());
			}
		}
	}

	void RenderTargetLines(const ImVec2& center, float radius, int degree, int distance, int alpha) {
		float targetRadian = degree * IM_PI / 180;
		for (float i = degree - 0.5f; i <= 0.5f + degree; i += 0.1f) {
			targetRadian = i * IM_PI / 180;
			//ImVec2 end = ImVec2(center.x + cos(targetRadian) * radius, center.y + sin(targetRadian) * radius);
			ImVec2 targetPosition = ImVec2(center.x + cos(targetRadian) * distance, center.y + sin(targetRadian) * distance);
			ImGui::GetWindowDrawList()->AddLine(center, targetPosition, IM_COL32(23, 235, 17, alpha), 1.0f);
			//ImGui::GetWindowDrawList()->AddCircleFilled(targetPosition, 8.0f, IM_COL32(23, 235, 17, alpha));
		}
	}

	void AddTargetToTable(const Target& target) {
		TableEntry entry;
		entry.id = counter;
		counter++;
		entry.distance = target.distance;
		entry.degree = target.degree;
		entry.time = GetCurrentTime();
		tableEntries.push_back(entry);
	}

};

void embraceTheDarkness()
{
	ImVec4* colors = ImGui::GetStyle().Colors;
	colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
	colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.19f, 0.19f, 0.19f, 0.92f);
	colors[ImGuiCol_Border] = ImVec4(0.19f, 0.19f, 0.19f, 0.29f);
	colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.24f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.40f, 0.54f);
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
	colors[ImGuiCol_CheckMark] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
	colors[ImGuiCol_Button] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
	colors[ImGuiCol_Header] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 0.00f, 0.00f, 0.36f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.20f, 0.22f, 0.23f, 0.33f);
	colors[ImGuiCol_Separator] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
	colors[ImGuiCol_SeparatorActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
	colors[ImGuiCol_Tab] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.20f, 0.20f, 0.36f);
	colors[ImGuiCol_TabUnfocused] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	colors[ImGuiCol_DockingPreview] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
	colors[ImGuiCol_DockingEmptyBg] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotLines] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogram] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_TableHeaderBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
	colors[ImGuiCol_TableBorderStrong] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
	colors[ImGuiCol_TableBorderLight] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
	colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
	colors[ImGuiCol_TextSelectedBg] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
	colors[ImGuiCol_DragDropTarget] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
	colors[ImGuiCol_NavHighlight] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 0.00f, 0.00f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg] = ImVec4(1.00f, 0.00f, 0.00f, 0.20f);
	colors[ImGuiCol_ModalWindowDimBg] = ImVec4(1.00f, 0.00f, 0.00f, 0.35f);

	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowPadding = ImVec2(8.00f, 8.00f);
	style.FramePadding = ImVec2(5.00f, 2.00f);
	style.CellPadding = ImVec2(6.00f, 6.00f);
	style.ItemSpacing = ImVec2(6.00f, 6.00f);
	style.ItemInnerSpacing = ImVec2(6.00f, 6.00f);
	style.TouchExtraPadding = ImVec2(0.00f, 0.00f);
	style.IndentSpacing = 25;
	style.ScrollbarSize = 15;
	style.GrabMinSize = 10;
	style.WindowBorderSize = 1;
	style.ChildBorderSize = 1;
	style.PopupBorderSize = 1;
	style.FrameBorderSize = 1;
	style.TabBorderSize = 1;
	style.WindowRounding = 7;
	style.ChildRounding = 4;
	style.FrameRounding = 3;
	style.PopupRounding = 4;
	style.ScrollbarRounding = 9;
	style.GrabRounding = 3;
	style.LogSliderDeadzone = 4;
	style.TabRounding = 4;
}

void on_message(struct mosquitto* mosq, void* obj, const struct mosquitto_message* message)
{
	if (message->payloadlen) {
		std::string payload(static_cast<char*>(message->payload), message->payloadlen);

		// Parse the JSON data
		boost::json::value jsonData = boost::json::parse(payload);

		// Extract the distance and degree data
		int distance = jsonData.at("distance").as_int64();
		int degree = jsonData.at("degree").as_int64();

		// Add the data to the vectar vector
		Target target;
		target.distance = distance;
		target.degree = degree;
		targets.push_back(target);
		vec[degree].second = distance;
		if (!sweep) {
			if (distance) {
				deg = degree;
				dist = distance;
			}
		}
	}
}

void sendMQTTMessage(struct mosquitto* mosq, const std::string& direction) {
	// Construct the message
	std::string message = direction;

	// Publish the message to the "radar/control" topic
	mosquitto_publish(mosq, NULL, "radar/control", message.size(), message.data(), 0, false);
}

void on_connect(struct mosquitto* mosq, void* obj, int result)
{
	if (!result) {
		mosquitto_subscribe(mosq, NULL, "radar/data", 0);
	}
	else {
		fprintf(stderr, "Connect failed\n");
	}
}


Walnut::Application* Walnut::CreateApplication(int argc, char** argv)
{
	Walnut::ApplicationSpecification spec;
	spec.Name = "Radar";
	Walnut::Application* app = new Walnut::Application(spec);
	app->PushLayer<ExampleLayer>();
	return app;
}
