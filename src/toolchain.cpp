// src/toolchain.cpp
//
// Cross-platform discovery of CMake, Git, and C++ compilers (MSVC, Clang, GCC/MinGW, Zig C++).

#include <numkit/sys/toolchain.hpp>
#include <numkit/sys/process.hpp>
#include <numkit/sys/env.hpp>

#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

namespace numkit::sys {

namespace {

std::string firstLine(const std::string &s) {
    size_t pos = s.find_first_of("\r\n");
    return (pos == std::string::npos) ? s : s.substr(0, pos);
}

ToolInfo probeCandidate(const std::string &name, const std::string &exePath, const std::string &versionArg = "--version", const std::string &extraArgs = "") {
    ToolInfo info;
    info.name = name;
    info.path = exePath;
    info.extraArgs = extraArgs;

    std::string cmd = "\"" + exePath + "\"";
    if (!extraArgs.empty()) cmd += " " + extraArgs;
    if (!versionArg.empty()) cmd += " " + versionArg;

    auto res = runProcess(cmd);
    if (res.ok()) {
        info.found = true;
        info.version = firstLine(res.output);
    }
    return info;
}

} // namespace

ToolInfo findCMake() {
    if (isCommandAvailable("cmake")) {
        auto info = probeCandidate("cmake", "cmake");
        if (info.found) return info;
    }

#ifdef _WIN32
    const char *candidates[] = {
        "C:\\Program Files\\CMake\\bin\\cmake.exe",
        "C:\\Program Files (x86)\\CMake\\bin\\cmake.exe",
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\CMake\\bin\\cmake.exe",
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\CMake\\bin\\cmake.exe",
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\CMake\\bin\\cmake.exe",
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\BuildTools\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\CMake\\bin\\cmake.exe",
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Professional\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\CMake\\bin\\cmake.exe",
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\CMake\\bin\\cmake.exe",
        "C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\BuildTools\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\CMake\\bin\\cmake.exe"
    };
#else
    const char *candidates[] = {
        "/usr/local/bin/cmake",
        "/opt/homebrew/bin/cmake",
        "/usr/bin/cmake",
        "/snap/bin/cmake"
    };
#endif

    std::error_code ec;
    for (const char *c : candidates) {
        if (fs::exists(c, ec) && fs::is_regular_file(c, ec)) {
            ToolInfo info;
            info.found = true;
            info.name = "cmake";
            info.path = c;
            return info;
        }
    }

    return {};
}

ToolInfo findGit() {
    if (isCommandAvailable("git")) {
        ToolInfo info;
        info.found = true;
        info.name = "git";
        info.path = "git";
        return info;
    }

#ifdef _WIN32
    const char *candidates[] = {
        "C:\\Program Files\\Git\\cmd\\git.exe",
        "C:\\Program Files\\Git\\bin\\git.exe",
        "C:\\Program Files (x86)\\Git\\cmd\\git.exe"
    };
#else
    const char *candidates[] = {
        "/usr/bin/git",
        "/usr/local/bin/git",
        "/opt/homebrew/bin/git"
    };
#endif

    std::error_code ec;
    for (const char *c : candidates) {
        if (fs::exists(c, ec) && fs::is_regular_file(c, ec)) {
            ToolInfo info;
            info.found = true;
            info.name = "git";
            info.path = c;
            return info;
        }
    }

    return {};
}

std::vector<ToolInfo> findAllCxxCompilers() {
    std::vector<ToolInfo> result;
    std::error_code ec;

#ifdef _WIN32
    // 1. Check MSVC via vswhere or standard VS 2022/2019 paths
    const char *vswhere = "C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe";
    if (fs::exists(vswhere, ec)) {
        auto res = runProcess("\"" + std::string(vswhere) + "\" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath");
        if (res.ok()) {
            std::string vsPath = firstLine(res.output);
            if (!vsPath.empty() && fs::exists(vsPath, ec)) {
                ToolInfo msvc;
                msvc.found = true;
                msvc.name = "msvc";
                msvc.path = vsPath;
                result.push_back(std::move(msvc));
            }
        }
    }
#endif

    // 2. Check clang++
    if (isCommandAvailable("clang++")) {
        auto info = probeCandidate("clang++", "clang++");
        if (info.found) result.push_back(std::move(info));
    }

    // 3. Check g++ (GCC / MinGW)
    if (isCommandAvailable("g++")) {
        auto info = probeCandidate("g++", "g++");
        if (info.found) result.push_back(std::move(info));
    }

    // 4. Check zig c++
    if (isCommandAvailable("zig")) {
        auto info = probeCandidate("zig c++", "zig", "version", "c++");
        if (info.found) result.push_back(std::move(info));
    }

    return result;
}

ToolInfo findCxxCompiler() {
    auto all = findAllCxxCompilers();
    if (!all.empty()) return all.front();
    return {};
}

} // namespace numkit::sys
