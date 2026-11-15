// include/numkit/sys/process.hpp
//
// Cross-platform process execution and presence detection.
#pragma once

#include <keel/core.h>
#include <keel/string.h>

namespace numkit::sys {

struct ProcessResult {
    keel::i32 exitCode = -1;
    keel::String output;
    bool ok() const noexcept { return exitCode == 0; }
};

// Runs a system command synchronously, capturing combined stdout and stderr.
ProcessResult runProcess(keel::StringRef command, keel::Allocator *alloc = nullptr);

// Checks if an executable command is available in PATH / system.
bool isCommandAvailable(keel::StringRef commandName);

} // namespace numkit::sys

namespace numkit {
    using sys::ProcessResult;
    using sys::runProcess;
    using sys::isCommandAvailable;
} // namespace numkit
