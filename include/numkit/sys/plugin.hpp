// include/numkit/sys/plugin.hpp
//
// numkit::PluginContext — the plugin boundary protocol for NumKit modules.
#pragma once

#include <numkit/value/value.hpp>
#include <numkit/value/object.hpp>
#include <numkit/value/error.hpp>
#include <numkit/sys/process.hpp>
#include <numkit/sys/fs_context.hpp>

#include <keel/memory.h>
#include <keel/ref.h>

#include <functional>
#include <string>
#include <vector>

#ifndef NUMKIT_PLUGIN_ABI_VERSION
#  define NUMKIT_PLUGIN_ABI_VERSION 1
#endif

#if !defined(NUMKIT_PLUGIN_EXPORT)
  #if defined(_WIN32) || defined(__CYGWIN__)
    #if defined(NUMKIT_PLUGIN_BUILD)
      #define NUMKIT_PLUGIN_EXPORT __declspec(dllexport)
    #else
      #define NUMKIT_PLUGIN_EXPORT
    #endif
  #else
    #if defined(__GNUC__) && __GNUC__ >= 4
      #define NUMKIT_PLUGIN_EXPORT __attribute__((visibility("default")))
    #else
      #define NUMKIT_PLUGIN_EXPORT
    #endif
  #endif
#endif

namespace numkit {

namespace ops { class RngContext; }
class FigureManager;

#if defined(NUMKIT_CORE_BUILD)
class Engine;
#endif

class PluginContext;
class Environment;
struct CallContext;

// Signature of external functions registered into the engine
using ExternalFunc = std::function<void(keel::ArrayRef<Value> args,
                                        keel::usize nargout,
                                        keel::MutArrayRef<Value> outs,
                                        CallContext &ctx)>;

// PluginContext: host interface passed to numkit_plugin_init.
// Inherited by HostPluginContext in the host engine.
class PluginContext {
public:
    virtual ~PluginContext() = default;
    virtual keel::Allocator *allocator() const noexcept = 0;
    virtual void registerFunction(const std::string &ns, const std::string &name, ExternalFunc func) = 0;
    virtual void registerFunction(const std::string &name, ExternalFunc func) = 0;
    virtual Value callFunctionHandle(const Value &handle, keel::ArrayRef<Value> args, Environment *env = nullptr) {
        (void)handle; (void)args; (void)env;
        return Value();
    }
    virtual std::vector<Value> callFunctionHandleMulti(const Value &handle, keel::ArrayRef<Value> args, keel::usize nargout, Environment *env = nullptr) {
        (void)handle; (void)args; (void)nargout; (void)env;
        return {};
    }
    virtual keel::u32 abiVersion() const noexcept { return NUMKIT_PLUGIN_ABI_VERSION; }

    virtual sys::ProcessResult runProcess(const std::string &command) {
        return sys::runProcess(command);
    }

    virtual ops::RngContext &rng() = 0;
    virtual FsContext::ResolvedPath resolvePath(const std::string &userPath) const = 0;
    virtual FigureManager &figureManager() = 0;
    virtual void registerBuiltinMSource(const std::string &src) { (void)src; }
    virtual const ExternalFunc *findExternal(const std::string &name, const Environment *env = nullptr) const {
        (void)name; (void)env;
        return nullptr;
    }
    virtual void registerClass(BuiltinClass cls) { (void)cls; }
    virtual const BuiltinClass *findClass(const std::string &name) const {
        (void)name;
        return nullptr;
    }
    virtual void assignToCaller(keel::i32 n, const std::string &name, Value val) {
        (void)n; (void)name; (void)val;
    }
};

#if !defined(NUMKIT_CORE_BUILD)
using Engine = PluginContext;
#endif

struct CallContext
{
    Engine *engine{nullptr};
    Environment *env{nullptr};
};

} // namespace numkit

// Entry point type of every dynamic module:
// extern "C" NUMKIT_PLUGIN_EXPORT bool numkit_plugin_init(numkit::PluginContext &ctx);
typedef bool (*NumkitPluginInitFn)(numkit::PluginContext &ctx);
