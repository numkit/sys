// src/toolchain.cpp
//
// Cross-platform discovery of CMake, Ninja, Git, and C++ compilers
// (Zig C++, MSVC, Clang, GCC/MinGW), including ~/.numkit/toolchain/.

#include <numkit/sys/toolchain.hpp>
#include <numkit/sys/process.hpp>
#include <numkit/sys/env.hpp>

#include <keel/fs.h>

#include <vector>

namespace numkit::sys {

namespace {

std::string firstLine(const std::string &s) {
    keel::usize pos = s.find_first_of("\r\n");
    return (pos == std::string::npos) ? s : s.substr(0, pos);
}

std::string joinSeg(const std::string &a, const std::string &b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    if (a.back() == '/' || a.back() == '\\') return a + b;
    return a + "/" + b;
}

bool isRegularFile(const std::string &p) noexcept {
    auto st = keel::fs_stat(keel::StringRef(p));
    return st.is_ok() && st.value().is_file;
}

std::string userNumkitPrefix() {
    std::string custom = envGet("NUMKIT_PREFIX");
    if (!custom.empty()) return custom;
#ifdef _WIN32
    std::string home = envGet("USERPROFILE");
    if (home.empty()) {
        home = envGet("HOMEDRIVE") + envGet("HOMEPATH");
    }
#else
    std::string home = envGet("HOME");
#endif
    if (home.empty()) return ".numkit";
    return joinSeg(home, ".numkit");
}

std::string exeName(const char *base) {
#ifdef _WIN32
    return std::string(base) + ".exe";
#else
    return std::string(base);
#endif
}

std::string quoteCmdPath(const std::string &p) {
#ifdef _WIN32
    std::string out = "\"";
    for (char ch : p) {
        if (ch == '"')
            out += "\\\"";
        else
            out.push_back(ch);
    }
    out.push_back('"');
    return out;
#else
    std::string out = "'";
    for (char ch : p) {
        if (ch == '\'')
            out += "'\\''";
        else
            out.push_back(ch);
    }
    out.push_back('\'');
    return out;
#endif
}

ToolInfo probeCandidate(const std::string &name, const std::string &exePath,
                        const std::string &versionArg = "--version",
                        const std::string &extraArgs = "") {
    ToolInfo info;
    info.name = name;
    info.path = exePath;
    info.extraArgs = extraArgs;

    for (unsigned char ch : exePath) {
        if (ch < 0x20 || ch == 0x7F)
            return info;
    }

    std::string cmd = quoteCmdPath(exePath);
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
    std::string prefix = userNumkitPrefix();
    const std::string portableCandidates[] = {
        joinSeg(prefix, "toolchain/cmake/bin/" + exeName("cmake")),
        joinSeg(prefix, "toolchain/cmake/CMake.app/Contents/bin/cmake"),
        joinSeg(prefix, "bin/" + exeName("cmake"))
    };
    for (const auto &p : portableCandidates) {
        if (isRegularFile(p)) {
            ToolInfo info;
            info.found = true;
            info.name = "cmake";
            info.path = p;
            return info;
        }
    }

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

    for (const char *c : candidates) {
        if (isRegularFile(c)) {
            ToolInfo info;
            info.found = true;
            info.name = "cmake";
            info.path = c;
            return info;
        }
    }

    return {};
}

ToolInfo findNinja() {
    std::string prefix = userNumkitPrefix();
    const std::string portableCandidates[] = {
        joinSeg(prefix, "toolchain/ninja/" + exeName("ninja")),
        joinSeg(prefix, "bin/" + exeName("ninja"))
    };
    for (const auto &p : portableCandidates) {
        if (isRegularFile(p)) {
            ToolInfo info;
            info.found = true;
            info.name = "ninja";
            info.path = p;
            return info;
        }
    }

    if (isCommandAvailable("ninja")) {
        auto info = probeCandidate("ninja", "ninja");
        if (info.found) return info;
    }

#ifdef _WIN32
    const char *candidates[] = {
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\Ninja\\ninja.exe",
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\Ninja\\ninja.exe",
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\Ninja\\ninja.exe",
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\BuildTools\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\Ninja\\ninja.exe"
    };
#else
    const char *candidates[] = {
        "/usr/local/bin/ninja",
        "/opt/homebrew/bin/ninja",
        "/usr/bin/ninja"
    };
#endif

    for (const char *c : candidates) {
        if (isRegularFile(c)) {
            ToolInfo info;
            info.found = true;
            info.name = "ninja";
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

    for (const char *c : candidates) {
        if (isRegularFile(c)) {
            ToolInfo info;
            info.found = true;
            info.name = "git";
            info.path = c;
            return info;
        }
    }

    return {};
}

ToolInfo findZig() {
    std::string prefix = userNumkitPrefix();
    const std::string portableCandidates[] = {
        joinSeg(prefix, "toolchain/zig/" + exeName("zig")),
        joinSeg(prefix, "bin/" + exeName("zig"))
    };
    for (const auto &p : portableCandidates) {
        if (isRegularFile(p)) {
            auto info = probeCandidate("zig c++", p, "version", "c++");
            if (info.found) return info;
        }
    }

    if (isCommandAvailable("zig")) {
        auto info = probeCandidate("zig c++", "zig", "version", "c++");
        if (info.found) return info;
    }

    return {};
}

std::vector<ToolInfo> findAllCxxCompilers() {
    std::vector<ToolInfo> result;

    if (auto zig = findZig(); zig.found) {
        result.push_back(std::move(zig));
    }

#ifdef _WIN32
    const char *vswhere = "C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe";
    if (keel::fs_exists(vswhere)) {
        auto res = runProcess("\"" + std::string(vswhere) + "\" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath");
        if (res.ok()) {
            std::string vsPath = firstLine(res.output);
            if (!vsPath.empty() && keel::fs_exists(vsPath)) {
                ToolInfo msvc;
                msvc.found = true;
                msvc.name = "msvc";
                msvc.path = vsPath;
                result.push_back(std::move(msvc));
            }
        }
    }
#endif

    if (isCommandAvailable("clang++")) {
        auto info = probeCandidate("clang++", "clang++");
        if (info.found) result.push_back(std::move(info));
    }

    if (isCommandAvailable("g++")) {
        auto info = probeCandidate("g++", "g++");
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
