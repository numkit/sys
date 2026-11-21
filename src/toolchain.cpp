// src/toolchain.cpp
//
// Cross-platform discovery of CMake, Ninja, Git, and C++ compilers
// (Zig C++, MSVC, Clang, GCC/MinGW), including ~/.numkit/toolchain/.

#include <numkit/sys/toolchain.hpp>
#include <numkit/sys/process.hpp>
#include <numkit/sys/env.hpp>

#include <keel/format.h>
#include <keel/fs.h>
#include <keel/process.h>
#include <keel/small_array.h>
#include <keel/string.h>

namespace numkit::sys {

namespace {

keel::String firstLine(keel::StringRef s) {
    for (keel::usize i = 0; i < s.size(); ++i) {
        if (s[i] == '\r' || s[i] == '\n')
            return keel::String(s.substr(0, i));
    }
    return keel::String(s);
}

keel::String joinSeg(keel::StringRef a, keel::StringRef b) {
    if (a.empty()) return keel::String(b);
    if (b.empty()) return keel::String(a);
    if (a.back() == '/' || a.back() == '\\') return keel::format("{}{}", a, b);
    return keel::format("{}/{}", a, b);
}

bool isRegularFile(keel::StringRef p) noexcept {
    auto st = keel::fs_stat(p);
    return st.is_ok() && st.value().is_file;
}

keel::String userNumkitPrefix() {
    keel::String custom = envGet("NUMKIT_PREFIX");
    if (!custom.empty()) return custom;
    auto home = keel::fs_home_dir();
    if (home.is_err() || home->empty()) return keel::String::from(".numkit");
    return joinSeg(*home, ".numkit");
}

keel::String exeName(keel::StringRef base) {
    return keel::format("{}{}", base, keel::process_exe_extension());
}

ToolInfo probeCandidate(keel::StringRef name, keel::StringRef exePath,
                        keel::StringRef versionArg = "--version",
                        keel::StringRef extraArgs = "") {
    ToolInfo info;
    info.name = keel::String(name);
    info.path = keel::String(exePath);
    info.extraArgs = keel::String(extraArgs);

    for (keel::usize i = 0; i < exePath.size(); ++i) {
        const auto ch = static_cast<keel::u8>(exePath[i]);
        if (ch < 0x20 || ch == 0x7F)
            return info;
    }

    keel::SmallArray<keel::StringRef, 8> argv;
    argv.push_back(exePath);
    keel::usize pos = 0;
    while (pos < extraArgs.size()) {
        while (pos < extraArgs.size() && extraArgs[pos] == ' ') ++pos;
        const keel::usize start = pos;
        while (pos < extraArgs.size() && extraArgs[pos] != ' ') ++pos;
        if (pos > start) argv.push_back(extraArgs.substr(start, pos - start));
    }
    if (!versionArg.empty()) argv.push_back(versionArg);

    keel::ProcessOptions opts;
    opts.capture_stdout = true;
    opts.capture_stderr = true;
    opts.merge_stderr = true;
    auto res = keel::process_run(argv.as_ref(), opts);
    if (res.is_ok() && res->ok()) {
        info.found = true;
        info.version = firstLine(res->stdout_text);
    }
    return info;
}

} // namespace

ToolInfo findCMake() {
    keel::String prefix = userNumkitPrefix();
    keel::String cmakeExe = exeName("cmake");
    const keel::String portableCandidates[] = {
        joinSeg(prefix, keel::format("toolchain/cmake/bin/{}", cmakeExe)),
        joinSeg(prefix, "toolchain/cmake/CMake.app/Contents/bin/cmake"),
        joinSeg(prefix, keel::format("bin/{}", cmakeExe))
    };
    for (const auto &p : portableCandidates) {
        if (isRegularFile(p)) {
            ToolInfo info;
            info.found = true;
            info.name = keel::String::from("cmake");
            info.path = p.clone();
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
            info.name = keel::String::from("cmake");
            info.path = keel::String::from(c);
            return info;
        }
    }

    return {};
}

ToolInfo findNinja() {
    keel::String prefix = userNumkitPrefix();
    keel::String ninjaExe = exeName("ninja");
    const keel::String portableCandidates[] = {
        joinSeg(prefix, keel::format("toolchain/ninja/{}", ninjaExe)),
        joinSeg(prefix, keel::format("bin/{}", ninjaExe))
    };
    for (const auto &p : portableCandidates) {
        if (isRegularFile(p)) {
            ToolInfo info;
            info.found = true;
            info.name = keel::String::from("ninja");
            info.path = p.clone();
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
            info.name = keel::String::from("ninja");
            info.path = keel::String::from(c);
            return info;
        }
    }

    return {};
}

ToolInfo findGit() {
    if (isCommandAvailable("git")) {
        ToolInfo info;
        info.found = true;
        info.name = keel::String::from("git");
        info.path = keel::String::from("git");
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
            info.name = keel::String::from("git");
            info.path = keel::String::from(c);
            return info;
        }
    }

    return {};
}

ToolInfo findZig() {
    keel::String prefix = userNumkitPrefix();
    keel::String zigExe = exeName("zig");
    const keel::String portableCandidates[] = {
        joinSeg(prefix, keel::format("toolchain/zig/{}", zigExe)),
        joinSeg(prefix, keel::format("bin/{}", zigExe))
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

keel::Array<ToolInfo> findAllCxxCompilers() {
    keel::Array<ToolInfo> result;

    if (auto zig = findZig(); zig.found) {
        result.push_back(keel::move(zig));
    }

#ifdef _WIN32
    const char *vswhere = "C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe";
    if (keel::fs_exists(vswhere)) {
        auto res = runProcess(keel::format("\"{}\" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath", vswhere));
        if (res.ok()) {
            keel::String vsPath = firstLine(res.output);
            if (!vsPath.empty() && keel::fs_exists(vsPath)) {
                ToolInfo msvc;
                msvc.found = true;
                msvc.name = keel::String::from("msvc");
                msvc.path = keel::move(vsPath);
                result.push_back(keel::move(msvc));
            }
        }
    }
#endif

    if (isCommandAvailable("clang++")) {
        auto info = probeCandidate("clang++", "clang++");
        if (info.found) result.push_back(keel::move(info));
    }

    if (isCommandAvailable("g++")) {
        auto info = probeCandidate("g++", "g++");
        if (info.found) result.push_back(keel::move(info));
    }

    return result;
}

ToolInfo findCxxCompiler() {
    auto all = findAllCxxCompilers();
    if (!all.empty()) return keel::move(all[0]);
    return {};
}

} // namespace numkit::sys
