// include/numkit/sys/branding.hpp
#pragma once

#include <cstdlib>
#include <string>

#ifdef _WIN32
#  include <stdlib.h>
#endif

namespace numkit::sys {

inline constexpr const char *kEnvPrefix = "NUMKIT";

inline std::string envVarName(const char *suffix)
{
    return std::string(kEnvPrefix) + "_" + suffix;
}

// Cross-platform environment-variable read: returns "" when unset.
inline std::string envGet(const char *name)
{
#ifdef _WIN32
    char *buf = nullptr;
    size_t len = 0;
    _dupenv_s(&buf, &len, name);
    std::string s = buf ? buf : "";
    std::free(buf);
    return s;
#else
    const char *v = std::getenv(name);
    return v ? v : "";
#endif
}

inline void envSet(const char *name, const std::string &value)
{
#ifdef _WIN32
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

// Reads environment variable with NUMKIT_<suffix>.
inline std::string envGetPrefixed(const char *suffix)
{
    return envGet(envVarName(suffix).c_str());
}

} // namespace numkit::sys

namespace numkit {
    using sys::kEnvPrefix;
    using sys::envVarName;
    using sys::envGet;
    using sys::envSet;
    using sys::envGetPrefixed;
} // namespace numkit
