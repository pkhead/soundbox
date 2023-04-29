{
    "targets": [
        {
            "target_name": "native",
            "sources": ["interface-src/binding.cpp"],
            "include_dirs": [
                "<!@(node -p \"require('node-addon-api').include\")",
                "deps/libsoundio/soundio"
            ],
            "libraries": ["../libsoundio.so.2.0.0"],
            # "libraries": ["../deps/libsoundio/build/libsoundio.a"],
            "cflags!": ["-fno-exceptions"],
            "cflags_cc!": ["-fno-exceptions"],
            "defines": ["NAPI_CPP_EXCEPTIONS"]
        }
    ]
}