// TODO:	shift graph to closest zero crossing to the middle
//			this may require storing audio data in some sort of ring buffer

#include <imgui.h>
#include "modules.h"
#include "../audio.h"

using namespace audiomod;

AnalyzerModule::AnalyzerModule() : ModuleBase(true)
{
	id = "effects.analyzer";
	name = "Analyzer";
}

AnalyzerModule::~AnalyzerModule() {
	if (left_channel) delete left_channel;
	if (right_channel) delete right_channel;
}

void AnalyzerModule::process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) {
	if (buffer_size != this->buf_size) {
		if (left_channel) delete left_channel;
		if (right_channel) delete right_channel;

		left_channel = new float[buffer_size / 2];
		right_channel = new float[buffer_size / 2];
		this->buf_size = buffer_size / 2;
	}

	size_t j = 0;
	for (size_t i = 0; i < buffer_size; i += channel_count) {
		left_channel[j] = 0;
		right_channel[j] = 0;
		output[i] = 0;
		output[i + 1] = 0;

		for (size_t k = 0; k < num_inputs; k++) {
			left_channel[j] += inputs[k][i];
			right_channel[j] += inputs[k][i + 1];

			output[i] += inputs[k][i];
			output[i + 1] += inputs[k][i + 1];
		}

		j++;
	}
}

void AnalyzerModule::_interface_proc() {
	ImGui::PlotLines(
		"###left_samples", 
		left_channel,
		buf_size,
		0,
		"L",
		-range,
		range,
		ImVec2(ImGui::GetTextLineHeight() * 15.0f, ImGui::GetTextLineHeight() * 10.0f)
	);

	ImGui::SameLine();
	ImGui::PlotLines(
		"###right_samples", 
		right_channel,
		buf_size,
		0,
		"R",
		-range,
		range,
		ImVec2(ImGui::GetTextLineHeight() * 15.0f, ImGui::GetTextLineHeight() * 10.0f)
	);
	
	ImGui::SameLine();
	ImGui::VSliderFloat("###range", ImVec2(ImGui::GetTextLineHeight() * 1.5f, ImGui::GetTextLineHeight() * 10.0f), &range, 0.1f, 1.0f, "");
	if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
		ImGui::SetTooltip("%.3f", range);
	}
}