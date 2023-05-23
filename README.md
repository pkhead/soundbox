# soundbox
![logo](logo.svg)

A cross-platform, free, and open-source digital audio workstation.

## Installation
Required libraries:
- GLFW3
- OpenGL
- X11 (Linux)

Prerequisites:
- CMake
- Visual Studio (Windows)
- Make (Linux)

On Windows, open "x64 Native Tools Command Prompt for Visual Studio" or "x86 Native Tools Command Prompt For Visual Studio", and `cd` to the project directory.

First, build libsoundio:
```bash
cd libsoundio
mkdir build && cd build

# GNU/Linux
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_STATIC_LIBS=off ..
make

# Windows
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_STATIC_LIBS=off -G "NMake Makefiles" ..
nmake
```

Then you can build soundbox:
```bash
mkdir build
cd build
cmake ..

# Linux
make

# Windows
msbuild soundbox.sln /property:Configuration=Debug
```
Then, to run the application, run `build/soundbox` on GNU/Linux or `build/Debug/soundbox.exe` on Windows.

## Credits
- [libsoundio](https://libsound.io) for the audio library
- Heavily influenced from [John Nesky](http://www.johnnesky.com/)'s [BeepBox](https://www.beepbox.co)