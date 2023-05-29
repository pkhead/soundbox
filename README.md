# soundbox
![logo](logo.svg)

A cross-platform, free, and open-source digital audio workstation.

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
- [libsoundio](https://libsound.io) for the audio library
- [GLFW](https://www.glfw.org/) for the graphics backend
- [John Nesky](http://www.johnnesky.com/)'s [BeepBox](https://www.beepbox.co)