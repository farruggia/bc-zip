---
layout: default
title: Compiling
---

# Compilation

bc-zip requires the following components:

- `cmake`
- A `C++11`-enabled compiler (`g++` 4.7+, `clang` 3.2+, `icc` 11+)
- Boost (uBLAS, program_options)
- A POSIX environment

The project is compiled like any standard CMake project:

- `cd` into the project's directory
- `mkdir build`
- `cd build`
- `cmake ../`
- `make`

The binary (`bc-zip`) will be located in the `build` directory.