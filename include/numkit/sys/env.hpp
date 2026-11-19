// include/numkit/sys/env.hpp
//
// Cross-platform environment variable access and manipulation.
#pragma once

#include <keel/env.h>
#include <keel/format.h>
#include <keel/string.h>

namespace numkit::sys {

inline constexpr const char *kEnvPrefix = "NUMKIT";

inline keel::String envVarName(keel::StringRef suffix, keel::Allocator *alloc = nullptr)
{
    return keel::format_alloc(alloc ? alloc : keel::default_allocator(), "{}_{}", kEnvPrefix, suffix);
}

// Cross-platform environment-variable read: returns "" when unset.
inline keel::String envGet(keel::StringRef name, keel::Allocator *alloc = nullptr)
{
    return keel::env_get_or_empty(name, alloc);
}

inline void envSet(keel::StringRef name, keel::StringRef value)
{
    (void)keel::env_set(name, value);
}

// Reads environment variable with NUMKIT_<suffix>.
inline keel::String envGetPrefixed(keel::StringRef suffix, keel::Allocator *alloc = nullptr)
{
    keel::SmallString<128> key;
    (void)keel::format_into(key, "{}_{}", kEnvPrefix, suffix);
    return envGet(key, alloc);
}

} // namespace numkit::sys

namespace numkit {
    using sys::kEnvPrefix;
    using sys::envVarName;
    using sys::envGet;
    using sys::envSet;
    using sys::envGetPrefixed;
} // namespace numkit
