# Specify the cross-compiler paths for Windows
set(CMAKE_SYSTEM_NAME Windows)

# Set Clang as the cross-compiler
set(CMAKE_C_COMPILER /opt/homebrew/opt/llvm/bin/clang)
set(CMAKE_CXX_COMPILER /opt/homebrew/opt/llvm/bin/clang++)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -O2")

# Use MinGW resource compiler, archiver, and linker
set(CMAKE_RC_COMPILER /opt/homebrew/bin/x86_64-w64-mingw32-windres)
set(CMAKE_AR /opt/homebrew/bin/x86_64-w64-mingw32-ar)
set(CMAKE_RANLIB /opt/homebrew/bin/x86_64-w64-mingw32-ranlib)

# Set MinGW-w64 g++ as the linker
set(CMAKE_LINKER /opt/homebrew/bin/x86_64-w64-mingw32-g++)

# Ensure that MinGW-w64 libraries and headers are being used
set(CMAKE_FIND_ROOT_PATH /opt/homebrew/opt/mingw-w64/toolchain-x86-64/mingw)

# Set Windows-specific compilation flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DWINVER=0x0601 -D_WIN32_WINNT=0x0601")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWINVER=0x0601 -D_WIN32_WINNT=0x0601")

# Adjust linker flags: Remove unsupported flags for Clang
set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++ -static")

# Restrict CMake to search in the specified MinGW-w64 root path for libraries and headers
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Enable verbose makefile for detailed build output (optional)
set(CMAKE_VERBOSE_MAKEFILE ON)
