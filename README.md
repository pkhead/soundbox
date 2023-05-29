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
Make sure the C++ and CMake Tools components have been installed through the Visual Studio Installer.
It will automatically configure CMake once this project is opened.

Select `soundbox.exe` as the startup item.

## Credits
- [libsoundio](https://libsound.io) for the audio library
- [GLFW](https://www.glfw.org/) for the graphics backend
- Heavily influenced by [John Nesky](http://www.johnnesky.com/)'s [BeepBox](https://www.beepbox.co)