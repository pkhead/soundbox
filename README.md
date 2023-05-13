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

### Linux
```
mkdir build
cd build
cmake ..
make
```
Then, to run the application, run `build/soundbox`.

### Windows
Open "x64 Native Tools Command Prompt for Visual Studio" or "x86 Native Tools Command Prompt For Visual Studio", and `cd` to the project directory. Then, run the following commands:
```
mkdir build
cd build
cmake ..
msbuild soundbox.sln /property:Configuration=Debug
```
The binary is located at `build/Debug/soundbox.exe`.

## Credits
- [libsoundio](https://libsound.io) for the audio library
- Heavily influenced from [John Nesky](http://www.johnnesky.com/)'s [BeepBox](https://www.beepbox.co)