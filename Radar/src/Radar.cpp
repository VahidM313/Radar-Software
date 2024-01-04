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

const char* mqttBroker = "broker.emqx.io";
const char* mqttTopic = "radar/data";

struct mqtt_userdata {
	int distance;
	int degree;
};

struct mosquitto* mosq;

void messageCallback(struct mosquitto* mosq, void* obj, const struct mosquitto_message* msg) {
	struct mqtt_userdata* userdata = (struct mqtt_userdata*)obj;

	// Parse the received JSON payload
	sscanf((const char*)msg->payload, "{\"distance\":%d,\"degree\":%d}", &userdata->distance, &userdata->degree);
}

void embraceTheDarkness();

struct TableEntry {
	unsigned long id;
	int distance;
	int degree;
	std::string time;
};

struct Target {
	int distance;
	int degree;
	bool isVisible;
	int count;
};

int dist{}, deg{};

typedef std::vector<Target> vectar;

vectar targets = {
	{200,45,true,0},
	{34,87,true,0},
	{127,134,true,0},
	{356,185,true,0},
	{5,222,true,0},
	{85,269,true,0},
	{265,350,true,0},
};

std::vector<TableEntry> tableEntries;

class ExampleLayer : public Walnut::Layer {
public:
	virtual void OnUIRender() override {
		embraceTheDarkness();
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(17.0f / 255.0f, 17.0f / 255.0f, 17.0f / 255.0f, 1.0f));
		ImGui::Begin("connection");
		if (ImGui::Button("generate")) {
			GenerateRandomTargets(30);
		}
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

private:
	int counter{};
	void GenerateRandomTargets(int count) {
		for (int i = 0; i < count; i++) {
			Target target;
			target.distance = std::rand() % 360; // Random distance between 0 and 500
			target.degree = std::rand() % 360; // Random degree between 0 and 360
			target.isVisible = true;  // All targets are initially visible
			target.count = 0;
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

		const int numberOfLine = 1000;
		const int mainLineNum = static_cast<int>(numberOfLine * 0.009);
		const int fadeAlpha{ 20 };

		for (int i = 0; i < numberOfLine; i++) {
			float radian = (angle - i * (30.0 / numberOfLine)) * IM_PI / 180;
			ImVec2 end = ImVec2(center.x + cos(radian) * radius, center.y + sin(radian) * radius);
			int alpha{};
			if (i <= mainLineNum) {
				alpha = 255;
			}
			else {
				alpha = static_cast<int>(fadeAlpha * exp(-3.0 * (i - mainLineNum) / numberOfLine));
			}
			ImGui::GetWindowDrawList()->AddLine(center, end, IM_COL32(255, 187, 38, alpha), 2.0f);
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

		RenderDistanceCircles(center, radius, 50, 450);

		for (auto& target : targets) {
			float targetRadian = target.degree * IM_PI / 180;
			float diff = angle - target.degree;
			if (diff < 0 && target.count == 1) {
				diff += 360;
			}
			diff = floor(diff * 10) / 10;
			if (diff > 0) {
				int alpha = static_cast<int>(255 - 255 * (diff / 360.0f));
				RenderTargetLines(center, radius, target.degree, target.distance, alpha);
			}
			if (diff == 0)
			{
				target.count++;
				if (target.count == 1) {
					AddTargetToTable(target);
					dist = target.distance;
					deg = target.degree;
				}
				if (target.count == 2)
					target.isVisible = false;
			}
		}

		ImGui::GetWindowDrawList()->AddCircleFilled(center, 5.0f, IM_COL32(255, 255, 255, 255, 255));

		angle += 0.1f;
		if (angle > 360.0f) angle = 0.0f;

		targets.erase(std::remove_if(targets.begin(), targets.end(), [](const Target& target) {
			return !target.isVisible;
			}), targets.end());
	}

	void RenderRadarLine(const ImVec2& center, float radius, float angle, int degree, bool isMainLine) {
		float radian = degree * IM_PI / 180;
		ImVec2 end = ImVec2(center.x + cos(radian) * radius, center.y + sin(radian) * radius);
		ImVec2 start = ImVec2(center.x + cos(radian) * (radius - (isMainLine ? 20 : 10)), center.y + sin(radian) * (radius - (isMainLine ? 20 : 10)));

		if (degree % 90 == 0) {
			ImGui::GetWindowDrawList()->AddLine(center, end, IM_COL32(255, 255, 255, 200), 3.0f);
		}
		else {
			ImGui::GetWindowDrawList()->AddLine(start, end, IM_COL32(255, 187, 38, 255), isMainLine ? 4.5f : 2.0f);
		}

		if (degree % 30 == 0) {
			ImVec2 textPos = ImVec2(end.x + 30 * cos(radian) - 10, end.y + 30 * sin(radian) - 8);
			ImGui::GetWindowDrawList()->AddText(textPos, IM_COL32(255, 255, 255, 255), std::to_string(degree).c_str());
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
			ImVec2 end = ImVec2(center.x + cos(targetRadian) * radius, center.y + sin(targetRadian) * radius);
			ImVec2 targetPosition = ImVec2(center.x + cos(targetRadian) * distance, center.y + sin(targetRadian) * distance);
			ImGui::GetWindowDrawList()->AddLine(targetPosition, end, IM_COL32(255, 187, 38, alpha), 1.0f);
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


Walnut::Application* Walnut::CreateApplication(int argc, char** argv)
{
	Walnut::ApplicationSpecification spec;
	spec.Name = "Radar";
	Walnut::Application* app = new Walnut::Application(spec);
	app->PushLayer<ExampleLayer>();
	return app;
}
