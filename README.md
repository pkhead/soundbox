# soundbox
![logo](logo.svg)

A free and cross-platform tool for making music. Essentially [BeepBox](https://www.beepbox.co/) if it were a native application.

## Features
- Pattern-based grid sequencer
- Variable-length patterns
- Support for VST, LADSPA, LV2, and CLAP plugins
- FX Mixer
- Custom tuning
- Automation
- Song change markers (time signature or tuning)
- Sample instrument (including a slicer)
- Channel for playing audio clips
- Many built-in effects

To see which features are not currently implemented, consult [TODO.txt](TODO.txt).
Most features aren't implemented as this is still fairly early into development.

## Building
Optional dependencies:
- [lilv](http://drobilla.net/software/lilv.html) and [suil](http://drobilla.net/software/suil.html) for LV2 plugin support

### Linux
Install dependencies:
```bash
# ubuntu
apt install cmake liblilv-dev libsuil-dev
```

Clone repository and build the project
```bash
# clone repository
git clone --recursive https://github.com/pkhead/soundbox
cd soundbox

# build the project
mkdir build && cd build
cmake ..
make # depends on your build system

# run the program
./soundbox
```

### Windows (Visual Studio)
Make sure the C++ and CMake Tools components have been installed through the Visual Studio installer.
It will automatically configure CMake once the folder is opened through Visual Studio. Once open, select `soundbox.exe` as the startup item.

If you wish to use the VS Native Build Tools Command Prompt, it is recommended to use Ninja as the CMake generator as
that appears to be the default build system for the GUI version. To configure
CMake and build the project using Ninja, type in the following commands:
```powershell
cmake -GNinja ..
ninja
```
If you wish to use NMake, type in these commands instead:
```powershell
cmake -G "NMake Makefiles" ..
nmake
```
Every other command should be the same as the process on Linux.

### Mac
No build instructions are provided since I don't have a Mac. But I think that, with the use of Homebrew, you
should be able to follow the instructions for Linux.

If anyone reading this is on a Mac, I would appreciate it if you helped out by providing a way
to build this program.

## Contributing
Please report bugs and feature requests to the [GitHub issue tracker](https://github.com/pkhead/soundbox/issues).
Pull requests are welcome.

## Thanks to
- [PortAudio](http://www.portaudio.com/) for the audio library
- [GLFW](https://www.glfw.org/) for the graphics backend
- [John Nesky](http://www.johnnesky.com/)'s [BeepBox](https://www.beepbox.co)
