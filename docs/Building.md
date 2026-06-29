### General Build Instructions

The SiS project makes use of CMake and scripts to build binaries and generally assumes a Linux/Unix environment.
Some considerations have been made to have minimal conflicts with the host system, but this is not a goal.
Most compilers and modern windows machines should be supported due to minimal dependencies, but no guarantees are made.

#### Main SiS Binary

The main SiS binary makes use of a basic CMakePresets.json file, to build both native and cross-compiled binaries.

##### Presets
- **debug**, Build the binary in debug mode with no optimizations.

- **tdebug**, Build the binary and tests in debug mode, then automatically run the tests.

- **release**, Build the binary in release mode with `-O3` optimizations.

- **trelease**, Build the binary and tests in release mode, then automatically run the tests with `-O3` optimizations.

- **windows**, Cross-compile the binary from Linux for Windows in release mode with `-O3` optimizations (requires MinGw).

Example of a native build:
```bash
git clone https://gitlab.com/Salad.sh/SiS.git
cd SiS
cmake --preset release 
cmake --build --preset trelease 
```

Example of a cross-compiled build:
```bash
sudo pacman -S mingw-w64-gcc # Use the appropriate package manager for your distro.
git clone https://gitlab.com/Salad.sh/SiS.git
cd SiS
cmake --preset windows 
cmake --build --preset windows 
```
#### Building C++ Native Libraries

SiS supports linking Dynamic Libraries for more complex or performance-critical applications.
The base set of libraries are located in the [/stdlib/](../stdlib/) directory.
Building the base set of libraries is as simple as running the [/stdlib/build_stdlib.sh](../stdlib/build_stdlib.sh) script.
``` bash
./build_stdlib.sh                  # native platform only
./build_stdlib.sh --windows        # cross-compile for Windows (requires MinGW)
./build_stdlib.sh --all            # native + Windows
./build_stdlib.sh --install /path  # native + install to /path
./build_stdlib.sh --all --install /path/to/sis

```
To build custom libraries you can either
- Use the provided CMakeLists.txt in the [/stdlib/](../stdlib/).
- Create your own.

The provided CMakeLists.txt assumes the following layout:

```
stdlib/
├── CMakeLists.txt
└── dynamic
    └── demo
        ├── CMakeLists.txt
        └── demo.cpp
```
Each dynamic library has their own entry in [/stdlib/dynamic/](../stdlib/dynamic/) directory. The main entry file should have the same name as their dedicated directory.
Optionally a CMakeLists.txt file can be used to add additional dependencies or define more complex build configurations. Which is placed at the root of the library directory.
For an example CMakeLists.txt please refer to this [file](../stdlib/demo/CMakeLists.txt).

If instead you opt to create your own dedicated git repository for your library, you will need the following header files:
- [SisRegistry.h](../include/SisRegistry.h)
- [Environment.h](../include/Environment.h)
- [Value.h](../include/Value.h)
- [SisDynamicLibMacros.h](../include/SisDynamicLibMacros.h) (optional but recommended)

Including the above header files is mandatory for linking to the SiS binary.
See [LibraryCreation.md](../docs/DynamicLibs.md) for more information on how make dynamic libraries for SiS.

