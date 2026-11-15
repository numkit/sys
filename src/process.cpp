// src/process.cpp
//
// Cross-platform process launcher implementation.

#include <numkit/sys/process.hpp>

#include <keel/format.h>
#include <keel/string.h>

#include <cstdlib>

#if defined(__EMSCRIPTEN__)

namespace numkit::sys {

ProcessResult runProcess(keel::StringRef /*command*/, keel::Allocator *alloc)
{
    return {-1, keel::String("numkit::sys::runProcess: process execution is not supported in WebAssembly environment", alloc)};
}

bool isCommandAvailable(keel::StringRef /*commandName*/)
{
    return false;
}

} // namespace numkit::sys

#else

#include <cstdio>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #define popen_cmd _popen
  #define pclose_cmd _pclose
#else
  #include <sys/wait.h>
  #define popen_cmd ::popen
  #define pclose_cmd ::pclose
#endif

namespace numkit::sys {

ProcessResult runProcess(keel::StringRef command, keel::Allocator *alloc)
{
    keel::Allocator *a = alloc ? alloc : keel::default_allocator();
    ProcessResult r{-1, keel::String(a)};
    // Capture combined stdout and stderr.
    // On Windows, _popen invokes `cmd.exe /c <string>`, which strips the first
    // and last quote when the command starts with `"` and contains multiple
    // quoted arguments; wrapping the entire string in outer quotes preserves
    // inner quotes intact.
    keel::SmallString<512> fullCmd;
#ifdef _WIN32
    (void)keel::format_into(fullCmd, "\"{} 2>&1\"", command);
    FILE *pipe = popen_cmd(fullCmd.c_str(), "rt");
#else
    (void)keel::format_into(fullCmd, "{} 2>&1", command);
    FILE *pipe = popen_cmd(fullCmd.c_str(), "r");
#endif

    if (!pipe) {
        r.exitCode = -1;
        r.output = keel::format_alloc(a, "runProcess: failed to spawn process for command: {}", command);
        return r;
    }

    char buf[4096];
    while (std::fgets(buf, sizeof(buf), pipe)) {
        r.output.append(keel::StringRef(buf));
    }

    int status = pclose_cmd(pipe);
#ifdef _WIN32
    r.exitCode = status;
#else
    r.exitCode = (status != -1 && WIFEXITED(status)) ? WEXITSTATUS(status) : status;
#endif
    return r;
}

bool isCommandAvailable(keel::StringRef commandName)
{
    if (commandName.empty())
        return false;
    for (keel::usize i = 0; i < commandName.size(); ++i) {
        const auto ch = static_cast<unsigned char>(commandName[i]);
        if (ch < 0x20 || ch == 0x7F)
            return false;
    }
#ifdef _WIN32
    for (char ch : commandName) {
        if (ch == '"' || ch == '&' || ch == '|' || ch == ';' || ch == '<' ||
            ch == '>' || ch == '^' || ch == '%' || ch == '!' || ch == '`')
            return false;
    }
    keel::SmallString<256> probe;
    (void)keel::format_into(probe, "where \"{}\" >nul 2>nul", commandName);
    return std::system(probe.c_str()) == 0;
#else
    if (commandName[0] == '-')
        return false;
    keel::SmallString<256> quoted("'");
    for (char ch : commandName) {
        if (ch == '\'')
            quoted.append("'\\''");
        else
            quoted.push_back(ch);
    }
    quoted.push_back('\'');
    keel::SmallString<320> probe;
    (void)keel::format_into(probe, "command -v -- {} >/dev/null 2>&1", quoted);
    return std::system(probe.c_str()) == 0;
#endif
}

} // namespace numkit::sys

#endif
