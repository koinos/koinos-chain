# Koinos Chain

This program implements the chain microservice, the enforcement of consensus, for the Koinos Blockchain Framework.

### Project Structure

This project's structure follows the [Pitchfork](https://api.csswg.org/bikeshed/?force=1&url=https://raw.githubusercontent.com/vector-of-bool/pitchfork/develop/data/spec.bs) specification.

```
├── build/   # An ephemeral directory for building the project. Not checked in, but excluded via .gitignore.
├── include/ # Contains all public headers for the Koinos Chain.
├── src/     # Contains all source code and private headers for Koinos Chain.
├── tools/   # Contains additional tooling for Koinos Chain, primarily WASM test code.
└── tests/   # Contains tests for Koinos Chain.
```

### Building

Koinos Chain's build process is configured using CMake. Additionally, all dependencies are managed through Hunter, a CMake drive package manager for C/C++. This means that all dependencies are downloaded and built during configuration rather than relying on system installed libraries.

```
mkdir build
cd build
cmake -D CMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release --parallel
```

You can optionally run static analysis with Clang-Tidy during the build process. Static analysis is checked in CI and is required to pass before merging pull requests.

```
cmake -D CMAKE_BUILD_TYPE=Debug -D STATIC_ANALYSIS=ON ..
```

### Testing

Tests are built by default as target `chain_tests`. You can building them specifically with:

```
cmake --build . --config Release --parallel --target chain_tests
```

Tests can be invoked from the tests directiory within the build directory.

```
cd tests
./chain_tests
```

Tests can also be ran in parallel using CTest.

```
cd tests
ctest -j
```

You can also generate a coverage report.

```
cmake -D CMAKE_BUILD_TYPE=Debug -D COVERAGE=ON ..
cmake --build . --config Debug --parallel 3 --target coverage
```

You can run tests in different sanitizer profiles. Those profiles are None (Default), Address, Stack, and Thread. Currently, these are only known to work with clang, but may work with gcc with additional environment configuration.

```
cmake -D CMAKE_BUILD_TYPE=Debug -D SANITIZER=Address ..
cmake --build . --config Debug --parallel --target chain_tests
cd tests
ctest -j
```

### Formatting

Formatting of the source code is enforced by ClangFormat. If ClangFormat is installed, build targets will be automatically generated. You can review the library's code style by uploading the included `.clang-format` to https://clang-format-configurator.site/.

You can build `format.check` to check formattting and `format.fix` to attempt to automatically fix formatting. It is recommended to check and manually fix formatting as automatic formatting can unintentionally change code.

### Contributing

As an open source project, contributions are welcome and appreciated. Before contributing, please read our [Contribution Guidelines](CONTRIBUTING.md).
