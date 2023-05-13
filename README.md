# soundbox
![logo](logo.svg)

A free and open-source digital audio workstation.

## Installation
Prerequisites:
- CMake
- NMake (Windows)
- Make (Linux)

### Linux
```
cd build
cmake ..
make
```
Then, to run the binary, run `build/soundbox`.

### Windows
```
cd build
cmake -G "NMake Makefiles" ..
nmake
```
Then run soundbox.exe that is in the build directory.

## Credits
- [libsoundio](https://libsound.io) for the audio library
- Heavily influenced from [John Nesky](http://www.johnnesky.com/)'s [BeepBox](https://www.beepbox.co)