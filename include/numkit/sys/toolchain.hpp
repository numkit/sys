// include/numkit/sys/toolchain.hpp
//
// Cross-platform discovery of build tools (CMake, Git) and C++ compilers
// (MSVC, Clang, GCC/MinGW, Zig C++).
#pragma once

#include <string>
#include <vector>

namespace numkit::sys {

struct ToolInfo {
    bool found = false;
    std::string name;      // e.g. "cmake", "git", "msvc", "clang++", "g++", "zig c++"
    std::string path;      // Full path or executable command
    std::string version;   // First line of --version output if probed
    std::string extraArgs; // e.g. "c++" when name == "zig c++"
};

// Locate CMake in PATH or standard installation locations (Visual Studio, Program Files, Homebrew, etc.)
ToolInfo findCMake();

// Locate Git in PATH or standard installation locations
ToolInfo findGit();

// Locate the best available C++17 compiler (MSVC cl.exe, clang++, g++/MinGW, zig c++)
ToolInfo findCxxCompiler();

// Discover all available C++ compilers on the system
std::vector<ToolInfo> findAllCxxCompilers();

} // namespace numkit::sys
