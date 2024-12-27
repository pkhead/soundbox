#pragma once
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <util.hpp>

//#include "util.h"

//size_t convert_from_stereo(float* src, float** dest, size_t channel_count, size_t frames_per_buffer, bool interleave);
//void convert_to_stereo(float** src, float* dest, size_t channel_count, size_t frames_per_buffer, bool interleave);

namespace dsp
{
    constexpr double PI = 3.14159265358979323846;
    constexpr float PIf = 3.14159265358979323846f;

    inline float db_to_mult(float db) {
        return powf(10.0f, db / 10.0f);
    }

    inline double db_to_mult(double db) {
        return powf(10.0, db / 10.0);
    }

    inline bool is_zero_crossing(float prev, float next) {
        return (prev == 0.0f && next == 0.0f) || (util::sign(prev) != util::sign(next));
    }

    // 2nd-order IIR filters
    class FilterIIR2ndOrder
    {
    public:
        // [channel][order]
        float x[3]; // input
        float y[3]; // output

        // filter coefficients
        float a[3];
        float b[3];

        FilterIIR2ndOrder();

        void low_pass(float sample_rate, float frequency, float linear_gain);
        void high_pass(float sample_rate, float frequency, float linear_gain);
        void all_pass(float sample_rate, float frequency, float linear_gain);
        void peak(float sample_rate, float frequency, float linear_gain, float bandwidth);

        void low_shelf(float sample_rate, float frequency, float gain, float slope);
        void high_shelf(float sample_rate, float frequency, float gain, float slope);

        void process(float* sample);

        float attenuation(float hz, float sample_rate);
    }; // class FilterIIR2ndOrder

    class ADSR
    {
    public:
        float attack = 0.0f;
        float decay = 0.0f;
        float sustain = 1.0f;
        float release = 0.0f;

        ADSR();
        ADSR(float a, float d, float s, float r);

        class Instance
        {
        private:
            static constexpr float MINIMUM_LEVEL = 0.001f;

            float value = MINIMUM_LEVEL;
            uint8_t stage = 0; // 0 = init, 1 = attack, 2 = decay, 3 = sustain, 4 = release

            int samples_remaining = 0;
            float multiplier = 1.0f;
            //float lerp_from = 0.0f;
            //float lerp_to = 0.0f;

        public:
            Instance();

            inline bool is_released() const {
                return stage == 3;
            }

            /**
            * Computes an envelope
            * @param adsr Envelope parameters
            * @param out Output envelope value
            * @returns True if the note ended
            **/
            bool compute(int sample_rate, float& out, const ADSR& params);

            /**
            * Release the note
            **/
            void release(const ADSR& params); 
        };
    }; // class ADSR

    template <class T = float>
    class DelayLine
    {
    private:
        size_t index;
        size_t _max_size;
        T* buf;

    public:
        size_t delay;

        DelayLine()
        :   index(0),
            buf(nullptr),
            _max_size(0),
            delay(0)
        {}

        DelayLine(size_t max_size) : DelayLine()
        {
            resize(max_size);
        }

        ~DelayLine()
        {
            if (buf) delete[] buf;
        }

        DelayLine(const DelayLine<T> &src) = delete;
        DelayLine<T> &operator=(const DelayLine<T>&) = delete;

        DelayLine(DelayLine<T> &&src) noexcept :
            index(src.index),
            buf(src.buf),
            _max_size(src.max_size),
            delay(src.delay)
        {
            src.buf = nullptr;
        }

        DelayLine<T> &operator=(DelayLine<T> &&src) noexcept {
            index = src.index;
            _max_size = src._max_size;
            buf = src.buf;
            delay = src.delay;
            src.buf = nullptr;
            return *this;
        }

        void resize(size_t new_capacity)
        {
            if (buf) {
                delete[] buf;
            }

            _max_size = new_capacity;

            // create new zero-initialized array
            buf = new float[_max_size];
            memset(buf, 0, _max_size * sizeof(float));
        }

        void clear()
        {
            memset(buf, 0, _max_size * sizeof(float));
        }

        inline float read() {
            assert(index < _max_size);
            return buf[index];
        }

        inline void write(float v) {
            assert(index < _max_size);
            buf[index++] = v;
            if (index >= delay) index = 0;
        }

        inline size_t max_size() const { return _max_size; }
    }; // class DelayLine

    class VUMeter {
    private:
        static constexpr size_t BUFFER_SIZE = 1024;

        unsigned int sample_rate;
        float max;
        float level;
        float peak;

        size_t bufidx;
        float *buffer;

        int wait_length;

    public:
        VUMeter(unsigned int sample_rate, float db_max);
        ~VUMeter();

        void update(float value);

        /**
        * Returns current level, mapping [0, db_max] to [0, 1]
        **/
        inline float get_level() const { return level; }

        /**
        * Returns maximum level in the past second, mapping [0, db_max] to [0, 1]
        **/
        inline float get_peak() const { return peak; }
    }; // class VUMeter
}