// src/process.cpp
//
// Cross-platform process launcher implementation.

#include <numkit/sys/process.hpp>

#include <cstdlib>
#include <sstream>

#if defined(__EMSCRIPTEN__)

namespace numkit::sys {

ProcessResult runProcess(const std::string &/*command*/)
{
    return {-1, "numkit::sys::runProcess: process execution is not supported in WebAssembly environment"};
}

bool isCommandAvailable(const std::string &/*commandName*/)
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
  #define popen_cmd ::popen
  #define pclose_cmd ::pclose
#endif

namespace numkit::sys {

ProcessResult runProcess(const std::string &command)
{
    ProcessResult r;
    // Capture combined stdout and stderr
    std::string fullCmd = command + " 2>&1";
#ifdef _WIN32
    FILE *pipe = popen_cmd(fullCmd.c_str(), "rt");
#else
    FILE *pipe = popen_cmd(fullCmd.c_str(), "r");
#endif

    if (!pipe) {
        r.exitCode = -1;
        r.output = "runProcess: failed to spawn process for command: " + command;
        return r;
    }

    std::ostringstream ss;
    char buf[4096];
    while (std::fgets(buf, sizeof(buf), pipe)) {
        ss << buf;
    }

    r.exitCode = pclose_cmd(pipe);
    r.output = ss.str();
    return r;
}

bool isCommandAvailable(const std::string &commandName)
{
#ifdef _WIN32
    std::string probe = "where " + commandName + " >nul 2>nul";
    return std::system(probe.c_str()) == 0;
#else
    std::string probe = "command -v " + commandName + " >/dev/null 2>&1";
    return std::system(probe.c_str()) == 0;
#endif
}

} // namespace numkit::sys

#endif
