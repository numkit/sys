// include/numkit/sys/toolchain.hpp
//
// Cross-platform discovery of build tools (CMake, Ninja, Git, Zig) and C++ compilers
// (Zig C++, MSVC, Clang, GCC/MinGW), including portable installs in ~/.numkit/toolchain/.
#pragma once

#include <string>
#include <vector>

namespace numkit::sys {

struct ToolInfo {
    bool found = false;
    std::string name;      // e.g. "cmake", "ninja", "git", "zig c++", "msvc", "clang++", "g++"
    std::string path;      // Full path or executable command
    std::string version;   // First line of --version output if probed
    std::string extraArgs; // e.g. "c++" when name == "zig c++"
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
std::vector<ToolInfo> findAllCxxCompilers();

} // namespace numkit::sys
