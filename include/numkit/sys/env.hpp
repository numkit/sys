// include/numkit/sys/env.hpp
//
// Cross-platform environment variable access and manipulation.
#pragma once

#include <keel/format.h>
#include <keel/string.h>

#include <cstdlib>

#ifdef _WIN32
#  include <stdlib.h>
#endif

namespace numkit::sys {

inline constexpr const char *kEnvPrefix = "NUMKIT";

inline keel::String envVarName(keel::StringRef suffix, keel::Allocator *alloc = nullptr)
{
    return keel::format_alloc(alloc ? alloc : keel::default_allocator(), "{}_{}", kEnvPrefix, suffix);
}

// Cross-platform environment-variable read: returns "" when unset.
inline keel::String envGet(keel::StringRef name, keel::Allocator *alloc = nullptr)
{
    keel::SmallString<128> key(name);
#ifdef _WIN32
    char *buf = nullptr;
    size_t len = 0;
    _dupenv_s(&buf, &len, key.c_str());
    keel::String s = buf ? keel::String(buf, alloc) : keel::String(alloc);
    std::free(buf);
    return s;
#else
    const char *v = std::getenv(key.c_str());
    return v ? keel::String(v, alloc) : keel::String(alloc);
#endif
}

inline void envSet(keel::StringRef name, keel::StringRef value)
{
    keel::SmallString<128> key(name);
    keel::SmallString<256> val(value);
#ifdef _WIN32
    _putenv_s(key.c_str(), val.c_str());
#else
    setenv(key.c_str(), val.c_str(), 1);
#endif
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
