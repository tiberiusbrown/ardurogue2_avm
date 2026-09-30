# ardurogue2_avm

This project builds an AVM image using an installed AVM SDK.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DAVM_SDK_ROOT=/path/to/avm-sdk
cmake --build build --config RelWithDebInfo --target ardurogue2
```

The image is written to `build/ardurogue2.bin`. When built from the parent
`avm` repository, CMake builds and installs the SDK automatically before
building this project.
