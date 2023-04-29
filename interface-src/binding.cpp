#include <napi.h>
#include <soundio.h>
#include <sstream>

using namespace Napi;

static SoundIo* soundio;

struct OutputDeviceStruct {
    Napi::ObjectReference jsRef;
};

static void writeCallback(SoundIoOutStream* outStream, int frameCountMin, int frameCountMax) {
    //const SoundIoChannelLayout* layout = &outStream->layout;
    struct SoundIoChannelArea* areas;
    int framesLeft = frameCountMax;
    int err;

    OutputDeviceStruct* data = (OutputDeviceStruct*)outStream->userdata;

    while (framesLeft > 0) {
        int frameCount = framesLeft;

        if ((err = soundio_outstream_begin_write(outStream, &areas, &frameCount))) {
            fprintf(stderr, "err soundio_outstream_begin_write: %s\n", soundio_strerror(err));
            return;
        }

        if (!frameCount) break;

        try {
            size_t byteLength = frameCount * outStream->layout.channel_count;

            Napi::Env env = data->jsRef.Env();
            Napi::Value callback = data->jsRef.Get("callback");

            if (!callback.IsUndefined() && !callback.IsNull()) {
                Napi::Value ret = callback.As<Napi::Function>().Call({
                    Napi::Number::New(env, byteLength)
                });
                
                if (ret.IsBuffer()) {
                    Napi::Uint8Array buf = ret.As<Napi::Uint8Array>();

                    if (buf.ByteLength() != byteLength) {
                        fprintf(stderr, "byte lengths do not match");
                        return;
                    }

                    void* data = buf.Data();
                    memcpy(areas[0].ptr, data, byteLength);
                }
            }
            
        } catch (const Error& e) {
            // TODO: better error message
            fprintf(stderr, "parse error: %s\n", e.Message().c_str());
        }

        if ((err = soundio_outstream_end_write(outStream))) {
            fprintf(stderr, "err soundio_outstream_end_write: %s\n", soundio_strerror(err));
            return;
        }

        framesLeft -= frameCount;
    }
}

Napi::Object CreateOutputDevice(const Napi::CallbackInfo& info) {
    int sampleRate = 48000;
    SoundIoFormat format = SoundIoFormatFloat32NE;
    int numChannels = 2;
    
    if (info.Length() > 0) {
        Napi::Object options = info[0].As<Napi::Object>();

        if (options.Has("sampleRate")) sampleRate = options.Get("sampleRate").As<Napi::Number>().Int32Value();
        if (options.Has("channels")) numChannels = options.Get("channels").As<Napi::Number>().Int32Value();
        /*
        if (options.Has("format")) {
            const Napi::String formatOp = options.Get("format").As<Napi::String>();

            if (formatOp == std::string("float32"))
        }
        */
    }
    
    int defaultOutDeviceIndex = soundio_default_output_device_index(soundio);
    if (defaultOutDeviceIndex < 0) {
        throw Napi::Error::New(info.Env(), "error on soundio_default_output_device_index");
    }

    struct SoundIoDevice *device = soundio_get_output_device(soundio, defaultOutDeviceIndex);
    if (!device) {
        throw Napi::Error::New(info.Env(), "error on soundio_get_output_device");
    }

    const SoundIoChannelLayout* layout = soundio_channel_layout_get_default(numChannels);

    //Napi::Reference<Napi::Object> ref = Napi::ObjectReference::New(info.This().As<Napi::Object>(), 1);
    Napi::Object object = Napi::Object::New(info.Env());
    OutputDeviceStruct* data = new OutputDeviceStruct;
    data->jsRef = Napi::Persistent(object);
    
    SoundIoOutStream* outStream = soundio_outstream_create(device);
    outStream->sample_rate = sampleRate;
    outStream->layout = soundio_device_supports_layout(device, layout) ? *layout : device->layouts[0];
    outStream->format = format;
    outStream->userdata = data;
    outStream->write_callback = writeCallback; // NEED IMPLEMENT!

    int err;
    if ((err = soundio_outstream_open(outStream))) {
        std::stringstream msg;
        msg << "unable to open device: " << soundio_strerror(err);
        throw Napi::Error::New(info.Env(), msg.str());
    }

    if (outStream->layout_error) {
        std::stringstream msg;
        msg << "unable to set channel layout: " << soundio_strerror(outStream->layout_error);
        throw Napi::Error::New(info.Env(), msg.str());
    }
    
    if ((err = soundio_outstream_start(outStream))) {
        std::stringstream msg;
        msg << "unable to start device: " << soundio_strerror(err);
        throw Napi::Error::New(info.Env(), msg.str());
    }

    object.Set("sampleRate", Napi::Number::New(info.Env(), outStream->sample_rate));
    object.Set("channels", Napi::Number::New(info.Env(), outStream->layout.channel_count));
    object.Set("bytesPerSample", Napi::Number::New(info.Env(), outStream->bytes_per_sample));
    object.Set("bytesPerFrame", Napi::Number::New(info.Env(), outStream->bytes_per_frame));
    object.Set("format", Napi::Number::New(info.Env(), outStream->format));
    object.Set("streamPtr", Napi::Number::New(info.Env(), (size_t)outStream));
    object.Set("devicePtr", Napi::Number::New(info.Env(), (size_t)device));

    return object;
}

void CloseOutputDevice(const Napi::CallbackInfo& info) {
    if (info[0].IsObject()) {
        Napi::Object obj = info[0].As<Napi::Object>();
        soundio_outstream_destroy((SoundIoOutStream*)(obj.Get("streamPtr").As<Napi::Number>().Int64Value()));
        soundio_device_unref((SoundIoDevice*)(obj.Get("devicePtr").As<Napi::Number>().Int64Value()));
    }
}

void Close(const Napi::CallbackInfo& info) {
    soundio_destroy(soundio);
}

Napi::Object Init(Env env, Object exports) {
    int err;

    soundio = soundio_create();

    if (!soundio) {
        throw Napi::Error::New(env, "error: soundio_create()");
    }

    if ((err = soundio_connect(soundio))) {
        std::stringstream errMessage;
        errMessage << "error connecting: " << soundio_strerror(err);
        throw Napi::Error::New(env, errMessage.str());
    }

    soundio_flush_events(soundio);

    exports.Set("close", Function::New(env, Close));
    exports.Set("closeOutputDevice", Function::New(env, CloseOutputDevice));
    exports.Set("createOutputDevice", Function::New(env, CreateOutputDevice));
    
    return exports;
}

NODE_API_MODULE(addon, Init);