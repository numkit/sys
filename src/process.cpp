// src/process.cpp
//
// Cross-platform process launcher implementation (backed by keel::process_*).

#include <numkit/sys/process.hpp>

#include <keel/format.h>
#include <keel/process.h>
#include <keel/string.h>

namespace numkit::sys {

ProcessResult runProcess(keel::StringRef command, keel::Allocator *alloc)
{
    keel::Allocator *a = alloc ? alloc : keel::default_allocator();
    keel::ProcessOptions opts;
    opts.capture_stdout = true;
    opts.capture_stderr = true;
    opts.merge_stderr = true;

    auto res = keel::process_run_shell(command, opts, a);
    if (res.is_ok()) {
        return {res->exit_code, keel::move(res->stdout_text)};
    }
    if (res.error() == keel::ErrorCode::NotSupported) {
        return {-1, keel::String("numkit::sys::runProcess: process execution is not supported in this environment", a)};
    }
    return {-1, keel::format_alloc(a, "runProcess: failed to spawn process for command: {} ({})",
                                   command, keel::error_message(res.error()))};
}

bool isCommandAvailable(keel::StringRef commandName)
{
    return keel::process_which(commandName).has_value();
}

} // namespace numkit::sys
