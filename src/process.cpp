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
  #include <sys/wait.h>
  #define popen_cmd ::popen
  #define pclose_cmd ::pclose
#endif

namespace numkit::sys {

ProcessResult runProcess(const std::string &command)
{
    ProcessResult r;
    // Capture combined stdout and stderr.
    // On Windows, _popen invokes `cmd.exe /c <string>`, which strips the first
    // and last quote when the command starts with `"` and contains multiple
    // quoted arguments; wrapping the entire string in outer quotes preserves
    // inner quotes intact.
#ifdef _WIN32
    std::string fullCmd = "\"" + command + " 2>&1\"";
    FILE *pipe = popen_cmd(fullCmd.c_str(), "rt");
#else
    std::string fullCmd = command + " 2>&1";
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

    int status = pclose_cmd(pipe);
#ifdef _WIN32
    r.exitCode = status;
#else
    r.exitCode = (status != -1 && WIFEXITED(status)) ? WEXITSTATUS(status) : status;
#endif
    r.output = ss.str();
    return r;
}

bool isCommandAvailable(const std::string &commandName)
{
    if (commandName.empty())
        return false;
    for (unsigned char ch : commandName) {
        if (ch < 0x20 || ch == 0x7F)
            return false;
    }
#ifdef _WIN32
    for (char ch : commandName) {
        if (ch == '"' || ch == '&' || ch == '|' || ch == ';' || ch == '<' ||
            ch == '>' || ch == '^' || ch == '%' || ch == '!' || ch == '`')
            return false;
    }
    std::string probe = "where \"" + commandName + "\" >nul 2>nul";
    return std::system(probe.c_str()) == 0;
#else
    if (commandName[0] == '-')
        return false;
    std::string quoted = "'";
    for (char ch : commandName) {
        if (ch == '\'')
            quoted += "'\\''";
        else
            quoted.push_back(ch);
    }
    quoted.push_back('\'');
    std::string probe = "command -v -- " + quoted + " >/dev/null 2>&1";
    return std::system(probe.c_str()) == 0;
#endif
}

} // namespace numkit::sys

#endif
