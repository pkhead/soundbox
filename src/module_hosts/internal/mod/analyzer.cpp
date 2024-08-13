#include <audio_engine/audio_engine.hpp>
#include <math.h>
#include <util.hpp>
#include <dsp.hpp>
#include <imgui.h>
#include <log.hpp>

#include "analyzer.hpp"

using namespace hosts::internal;

// didn't bother making the ModuleBase automatically assign the idle callback, so
// doing it manually for now.
static void idle_callback(modules::AudioEngine &engine, modules::ModuleID id, void *userdata)
{
    AnalyzerModule *mod = static_cast<AnalyzerModule*>(userdata);
    mod->idle();
}

AnalyzerModule::AnalyzerModule(modules::ModuleCreator& mod) :
    ModuleBase(mod),
    queue_capacity((size_t)(mod.engine.sample_rate() * 0.5f) * 2), // hold 0.5 seconds of audio data
    audio_queue_left(queue_capacity),
    audio_queue_right(queue_capacity)
{
    mod.add_audio_input(2);
    mod.add_audio_output(2);

    audio_state.buf_left = new float[window_buffer_size];
    audio_state.buf_right = new float[window_buffer_size];
    audio_state.index = 0;

    ui_state.buf_left = new float[window_buffer_size];
    ui_state.buf_right = new float[window_buffer_size];

    memset(ui_state.buf_left, 0, window_buffer_size * sizeof(float));
    memset(ui_state.buf_right, 0, window_buffer_size * sizeof(float));

    mod.idle = idle_callback;

    /*complex_left = new fftwf_complex[arr_size];
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
    }*/
}

AnalyzerModule::~AnalyzerModule() {
    logger::log_debug("AnalyzerModule::~AnalyzerModule() called");

    //ready = false;
    delete[] audio_state.buf_left;
    delete[] audio_state.buf_right;
    delete[] ui_state.buf_left;
    delete[] ui_state.buf_right;

    /*delete[] complex_left;
    delete[] complex_right;
    delete[] real_left;
    delete[] real_right;

    for (int c = 0; c < 2; c++)
    {
        fftwf_free(fft_in[c]);
        fftwf_free(fft_out[c]);
        fftwf_destroy_plan(fft_plan[c]);
    }*/
}

void AnalyzerModule::process(modules::ModuleProcessor &proc)
{
    constexpr uint8_t channel_count = 2;

    float *input = proc.audio_input(0);
    float *output = proc.audio_output(0);
    auto &state = audio_state;

    //ring_buffer.write(output, proc.buffer_frame_count * channel_count);

    for (size_t i = 0; i < proc.buffer_frame_count * channel_count; i += channel_count) {
        output[i] = input[i];
        output[i+1] = input[i+1];

        state.buf_left[state.index] = input[i];
        state.buf_right[state.index] = input[i+1];
        state.index++;
        
        if (state.index >= window_buffer_size)
        {
            audio_queue_left.write(state.buf_left, window_buffer_size);
            audio_queue_right.write(state.buf_right, window_buffer_size);
            state.index = 0;
        }
    }

    // write to window
    /*if (!in_use && ring_buffer.queued() > samples_per_window)
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
    }*/

    //ready = true;
}

static int offset_zero_crossing(float* buf, size_t buf_size, size_t border)
{
    // search both left and right side for a zero crossing
    int origin = buf_size / 2;
    float cur[2], prev[2];

    for (int i = 0; i < border - 1; i++)
    {
        cur[0] = buf[origin - i]; // left side
        prev[0] = buf[origin - i - 1];
        cur[1] = buf[origin + i]; // right side
        prev[1] = buf[origin + i - 1];

        // if there is a rising zero crossing on the left side
        if (dsp::is_zero_crossing(prev[0], cur[0]) && cur[0] > prev[0])
            return -i;

        // if there is rising zero crossing on the right side
        else if (dsp::is_zero_crossing(prev[1], cur[1]) && cur[1] > prev[1])
            return i;
    }

    return 0;
}

// need to constantly read queue so that it doesn't run out of space.
// if it does, it takes a bit for the analyzer ui to display the correct information again.
void AnalyzerModule::idle()
{
    auto &state = ui_state;
    
    // read audio queue
    //if (audio_queue_left.available_for_read() >= window_buffer_size && audio_queue_right.available_for_read() >= window_buffer_size)
    //{
        audio_queue_left.read(state.buf_left, window_buffer_size);
        audio_queue_right.read(state.buf_right, window_buffer_size);
    //}
    //else
    //{
        //logger::log_warning("AnalyzerModule::ui: not enough audio data");
        //memset(ui_left, 0, frames_per_buffer * sizeof(float));
        //memset(ui_right, 0, frames_per_buffer * sizeof(float));
    //}
}

void AnalyzerModule::ui() {
    auto &state = ui_state;
    ImVec2 graph_size = ImVec2(ImGui::GetTextLineHeight() * 15.0f, ImGui::GetTextLineHeight() * 10.0f);

    int offset_left = offset_zero_crossing(state.buf_left, window_buffer_size, window_margin);
    int offset_right = offset_zero_crossing(state.buf_right, window_buffer_size, window_margin);

    ImGui::SetCursorPos(Vec2(ImGui::GetCursorPos()) + Vec2(0.0f, (ImGui::GetContentRegionAvail().y - graph_size.y) / 2.0f));
    ImGui::PlotLines("##Left", state.buf_left + offset_left + window_margin, frames_per_window, 0, nullptr, -1.0f, 1.0f, graph_size);
    ImGui::SameLine();
    ImGui::PlotLines("##Right", state.buf_right + offset_right + window_margin, frames_per_window, 0, nullptr, -1.0f, 1.0f, graph_size);
}