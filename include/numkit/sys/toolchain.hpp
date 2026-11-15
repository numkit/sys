// include/numkit/sys/toolchain.hpp
//
// Cross-platform discovery of build tools (CMake, Ninja, Git, Zig) and C++ compilers
// (Zig C++, MSVC, Clang, GCC/MinGW), including portable installs in ~/.numkit/toolchain/.
#pragma once

#include <keel/array.h>
#include <keel/string.h>

namespace numkit::sys {

struct ToolInfo {
    bool found = false;
    keel::String name;      // e.g. "cmake", "ninja", "git", "zig c++", "msvc", "clang++", "g++"
    keel::String path;      // Full path or executable command
    keel::String version;   // First line of --version output if probed
    keel::String extraArgs; // e.g. "c++" when name == "zig c++"
};

// Locate CMake in ~/.numkit/toolchain/, PATH, or standard installation locations
ToolInfo findCMake();

// Locate Ninja in ~/.numkit/toolchain/, PATH, or standard installation locations
ToolInfo findNinja();

// Locate Git in PATH or standard installation locations
ToolInfo findGit();

// Locate Zig ("zig c++") in ~/.numkit/toolchain/, ~/.numkit/bin/, or PATH
ToolInfo findZig();

// Locate the best available C++17 compiler (Zig C++, MSVC, clang++, g++/MinGW)
ToolInfo findCxxCompiler();

// Discover all available C++ compilers on the system
keel::Array<ToolInfo> findAllCxxCompilers();

} // namespace numkit::sys
