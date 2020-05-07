## Building the project on Linux
On **Linux**, run the following bash script from the root directory:
```bash
./scripts/build-linux.sh
```

Or, to build manually, follow these commands from the root directory:
```bash
mkdir build
cd build
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Release ..
ninja
```

To build for debug under Linux, add to the `-DCMAKE_BUILD_TYPE=Debug` flag when invoking `cmake`.

## Building the project on Windows
On **Windows**, open the project as a Visual Studio 2019 CMake project by selecting the CMakeLists.txt
file and then build the project.

After building the project, you can find a static and shared library in `bin`.
