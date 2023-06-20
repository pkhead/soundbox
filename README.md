# soundbox
![logo](logo.svg)

A cross-platform, free, and open-source digital audio workstation.

## Features
Anything BeepBox has, plus:

- Custom tuning
- Automation
- Song change markers (time signature or tuning)
- Variable-length patterns
- Plugin support (VST, LADSPA, LV2, CLAP)
- FX Mixer
- Sample instrument (including a slicer)
- Channel for playing audio clips 
- Many other effects

To see which features are not currently implemented, consult [TODO.txt](TODO.txt).
Most features aren't implemented as this is still fairly early into development.

## Installation
Prerequisites:
- CMake
- Visual Studio (Windows)
- Make (Linux)

### GNU/Linux
```bash
# initialize repository
git clone https://github.com/pkhead/soundbox
cd soundbox
git submodule init
git submodule update

# build the project
mkdir build && cd build
cmake ..
make

# run the program
./soundbox
```

### Windows (Visual Studio GUI)
Make sure the C++ and CMake Tools components have been installed through the Visual Studio installer.
It will automatically configure CMake once the folder is opened through Visual Studio.

Select `soundbox.exe` as the startup item.

### Windows (VS Native Build Tools Command Prompt)
Using the Ninja build system (check if it's installed):
```batch
Rem initialize repository
git clone https://github.com/pkhead/soundbox
cd soundbox
git submodule init
git submodule update

Rem build the project using Ninja
mkdir build && cd build
cmake -GNinja ..
ninja

Rem run the program
soundbox.exe
```
Using NMake:
```batch
Rem initialize repository, see above...

Rem build the project using NMake
mkdir build && cd build
cmake -G "NMake Makefiles" ..
nmake

Rem run the program
soundbox.exe
```


## Thanks to
- [PortAudio](http://www.portaudio.com/) for the audio library
- [GLFW](https://www.glfw.org/) for the graphics backend
- [John Nesky](http://www.johnnesky.com/)'s [BeepBox](https://www.beepbox.co)