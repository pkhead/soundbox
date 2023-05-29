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

First, make sure you have GLFW3 installed. If it's not installed in a standard library/header path,
then you can tell CMake to look at the needed directories by passing in `-DCMAKE_PREFIX_PATH=/path/to/glfw3`.

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
Assuming CMake is installed for Visual Studio, it will auto-configure once the folder is opened.

If GLFW is not installed on a standard search path, you can tell CMake to look for by adding
`-DCMAKE_PREFIX_PATH=C:\path\to\glfw3` in the project's CMakeSettings.json

## Credits
- [libsoundio](https://libsound.io) for the audio library
- Heavily influenced by [John Nesky](http://www.johnnesky.com/)'s [BeepBox](https://www.beepbox.co)