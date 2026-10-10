// include/numkit/sys/process.hpp
//
// Cross-platform process execution and presence detection.
#pragma once

#include <string>

namespace numkit::sys {

struct ProcessResult {
    int exitCode = -1;
    std::string output;
    bool ok() const noexcept { return exitCode == 0; }
};

// Runs a system command synchronously, capturing combined stdout and stderr.
ProcessResult runProcess(const std::string &command);

// Checks if an executable command is available in PATH / system.
bool isCommandAvailable(const std::string &commandName);

} // namespace numkit::sys

namespace numkit {
    using sys::ProcessResult;
    using sys::runProcess;
    using sys::isCommandAvailable;
} // namespace numkit
