#include <imgui.h>
#include <cmath>
#include "../util.h"
#include "../dsp.h"
#include "analyzer.h"

using namespace audiomod;

AnalyzerModule::AnalyzerModule(DestinationModule& dest) :
    ModuleBase(dest, true),
    ring_buffer(dest.sample_rate * 0.5) // hold 0.5 seconds of audio
{
    id = "effect.analyzer";
    name = "Analyzer";

    size_t arr_size = frames_per_window + window_margin * 2;
    buf = new float[arr_size * dest.channel_count];

    for (int i = 0; i < 2; i++) {
        window_left[i] = new float[arr_size];
        window_right[i] = new float[arr_size];
    }

    complex_left = new fftwf_complex[arr_size];
    complex_right = new fftwf_complex[arr_size];
    real_left = new float[arr_size];
    real_right = new float[arr_size];

    ready = false;

    // create fft data for each channel
    for (int c = 0; c < 2; c++)
    {
        fft_in[c] = (float*) fftwf_malloc(sizeof(float) * arr_size);
        fft_out[c] = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * arr_size);
        fft_plan[c] = fftwf_plan_dft_r2c_1d(arr_size, fft_in[c], fft_out[c], FFTW_ESTIMATE);
    }
}

AnalyzerModule::~AnalyzerModule() {
    ready = false;
    delete[] buf;

    for (int i = 0; i < 2; i++) {
        delete[] window_left[i];
        delete[] window_right[i];
    }

    delete[] complex_left;
    delete[] complex_right;
    delete[] real_left;
    delete[] real_right;

    for (int c = 0; c < 2; c++)
    {
        fftwf_free(fft_in[c]);
        fftwf_free(fft_out[c]);
        fftwf_destroy_plan(fft_plan[c]);
    }
}

void AnalyzerModule::process(float** inputs, float* output, size_t num_inputs, size_t buffer_size, int sample_rate, int channel_count) {
    for (size_t i = 0; i < buffer_size; i += channel_count) {
        output[i] = 0;
        output[i + 1] = 0;

        for (size_t k = 0; k < num_inputs; k++) {
            output[i] += inputs[k][i];
            output[i + 1] += inputs[k][i + 1];
        }
    }

    ring_buffer.write(output, buffer_size);
    size_t samples_per_window = (frames_per_window + window_margin * 2) * 2;

    // write to window
    if (!in_use && ring_buffer.queued() > samples_per_window)
    {
        int buf_idx = 1 - window_front;
        
        float* left = window_left[buf_idx];
        float* right = window_right[buf_idx];
        
        size_t num_read = ring_buffer.read(buf, samples_per_window);
        assert(num_read == samples_per_window);

        size_t j = 0;

        for (size_t i = 0; i < samples_per_window; i += channel_count) {
            left[j] = buf[i];
            right[j] = buf[i + 1];
            j++;
        }

        if (!in_use) window_front = buf_idx;
    }

    ready = true;
}

static int offset_zero_crossing(float* buf, size_t buf_size, size_t border)
{
    // search both left and right side for a zero crossing
    int origin = buf_size / 2;
    float cur[2], prev[2];

    for (int i = 0; i + origin < buf_size - border - 1 && i + origin > border; i++)
    {
        cur[0] = buf[origin - i]; // left side
        prev[0] = buf[origin - i - 1];
        cur[1] = buf[origin + i]; // right side
        prev[1] = buf[origin + i - 1];

        // if there is a rising zero crossing on the left side
        if (is_zero_crossing(prev[0], cur[0]) && cur[0] > prev[0])
            return -i;

        // if there is rising zero crossing on the right side
        else if (is_zero_crossing(prev[1], cur[1]) && cur[1] > prev[1])
            return i;
    }

    return 0;
}

void AnalyzerModule::_interface_proc() {
    // use placeholder if audio process isn't ready to show analysis
    float placeholder[2] = { 0.0f, 0.0f };
    bool ready = this->ready;
    in_use = true;

    ImVec2 graph_size = ImVec2(ImGui::GetTextLineHeight() * 15.0f, ImGui::GetTextLineHeight() * 10.0f);
    size_t frames_per_buffer = frames_per_window + window_margin * 2;
    size_t samples_per_buffer = frames_per_buffer * 2;

    // mode slider
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Oscilloscope");
    ImGui::SameLine();
    ImGui::RadioButton("##option-oscil", &mode, 0);
    ImGui::AlignTextToFramePadding();
    ImGui::SameLine();
    ImGui::Text("Spectrum");
    ImGui::SameLine();
    ImGui::RadioButton("##option-spect", &mode, 1);

    // show oscilloscope align checkbox
    if (mode == 0) {
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Align");
        ImGui::SameLine();
        ImGui::Checkbox("###align", &align);
    }

    if (ready)
    {
        float* left = window_left[window_front];
        float* right = window_right[window_front];

        if (mode == 0)
        {
            int offset_left = 0;
            int offset_right = 0;

            if (align) {
                // align display to nearest zero crossing from center
                offset_left = offset_zero_crossing(left, frames_per_buffer, window_margin);
                offset_right = offset_zero_crossing(right, frames_per_buffer, window_margin);
            }

            ImGui::PlotLines(
                "###left_samples", 
                left + window_margin + offset_left,
                frames_per_window,
                0,
                "L",
                -range,
                range,
                graph_size
            );

            ImGui::SameLine();
            ImGui::PlotLines(
                "###right_samples", 
                right + window_margin + offset_right,
                frames_per_window,
                0,
                "R",
                -range,
                range,
                graph_size
            );

            // show range slider
            ImGui::SameLine();
            ImGui::VSliderFloat(
                "###range",
                ImVec2(ImGui::GetTextLineHeight() * 1.5f, ImGui::GetTextLineHeight() * 10.0f),
                &range,
                0.01f,
                1.0f,
                "",
                ImGuiSliderFlags_Logarithmic);
            if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
                ImGui::SetTooltip("%.3f", range);
            }
        } else {
            // feed left/right buffers into fftw
            for (int i = 0; i < frames_per_buffer; i++)
            {
                fft_in[0][i] = left[i];
                fft_in[1][i] = right[i];
            }

            // perform analysis
            // TODO: this loop could probably be a little more optimized
            fftwf_execute(fft_plan[0]);
            fftwf_execute(fft_plan[1]);
            
            for (int i = 0; i < frames_per_buffer; i++)
            {
                float t = powf(
                    2.0f,
                    ((float)i * (logf((float)frames_per_buffer / 2.0f) / logf(2.0f)))
                    / (float)frames_per_buffer
                );

                int t0 = (int)t;

                float l =
                    fft_out[0][t0][0] * fft_out[0][t0][0] +
                    fft_out[0][t0][1] * fft_out[0][t0][1];
                float r =
                    fft_out[1][t0][0] * fft_out[1][t0][0] +
                    fft_out[1][t0][1] * fft_out[1][t0][1];

                real_left[i] = l;
                real_right[i] = r;

                /*
                this piece of code does interpolation
                so that at low delta-t's, it doesn't make square shapes

                int t0 = (int)t - 1;
                int t1 = (int)t;

                // sin(x)^2 + cos(x)^2 = 1 
                float l0 =
                    complex_left[t0].real * complex_left[t0].real +
                    complex_left[t0].imag * complex_left[t0].imag;
                float l1 = 
                    complex_left[t1].real * complex_left[t1].real +
                    complex_left[t1].imag * complex_left[t1].imag;
                float r0 =
                    complex_right[t0].real * complex_right[t0].real +
                    complex_right[t0].imag * complex_right[t0].imag;
                float r1 =
                    complex_right[t1].real * complex_right[t1].real +
                    complex_right[t1].imag * complex_right[t1].imag;

                float mod = fmodf(t, 1.0f);
                real_left[i] = (l1 - l0) * mod + l0;
                real_right[i] = (r1 - r0) * mod + l1;*/
            }

            // render analysis
            float scale = frames_per_window * 2.0f;
            ImGui::PlotLines(
                "###left_samples", 
                real_left,
                frames_per_buffer,
                0,
                "L",
                0,
                scale,
                graph_size
            );

            ImGui::SameLine();
            ImGui::PlotLines(
                "###right_samples", 
                real_right,
                frames_per_buffer,
                0,
                "R",
                0,
                scale,
                graph_size
            );
        }
    }
    else
    {
        // processing thread is not ready to show data,
        // display a placeholder instead
        ImGui::PlotLines(
            "###left_samples", 
            placeholder,
            2,
            0,
            "L",
            -range,
            range,
            graph_size
        );

        ImGui::SameLine();
        ImGui::PlotLines(
            "###right_samples", 
            placeholder,
            2,
            0,
            "R",
            -range,
            range,
            graph_size
        );
    }

    in_use = false;
}