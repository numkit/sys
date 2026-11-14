// include/numkit/sys/lite_context.hpp
//
// Engine-free `PluginContext` implementation for standalone toolbox execution.
// Depends ONLY on L0 (`numkit/value` and `numkit/sys`) — zero dependency on
// `numkit/ops` or `numkit/core` (no parser, AST, TreeWalker, or VM).
#pragma once

#include <numkit/sys/fs_context.hpp>
#include <numkit/sys/plugin.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/object.hpp>
#include <numkit/value/span.hpp>
#include <numkit/value/value.hpp>

#include <memory_resource>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace numkit::sys {

class LitePluginContext final : public PluginContext
{
public:
    explicit LitePluginContext(ops::RngContext *rng = nullptr,
                               std::pmr::memory_resource *mr = std::pmr::get_default_resource())
        : rng_(rng)
        , mr_(mr ? mr : std::pmr::get_default_resource())
    {
    }

    ~LitePluginContext() override = default;

    LitePluginContext(const LitePluginContext &) = delete;
    LitePluginContext &operator=(const LitePluginContext &) = delete;
    LitePluginContext(LitePluginContext &&) = delete;
    LitePluginContext &operator=(LitePluginContext &&) = delete;

    void setRng(ops::RngContext *rng) noexcept { rng_ = rng; }

    std::pmr::memory_resource *resource() const noexcept override { return mr_; }

    void registerFunction(const std::string &ns, const std::string &name, ExternalFunc func) override
    {
        if (name.empty() || !func) return;
        ExternalFunc wrapped = wrapWithSelf(std::move(func));
        if (!ns.empty()) {
            qualifiedFuncs_[ns + "." + name] = wrapped;
            auto dot = ns.find('.');
            if (dot != std::string::npos) {
                std::string topNs = ns.substr(0, dot);
                if (!topNs.empty()) {
                    qualifiedFuncs_[topNs + "." + name] = wrapped;
                }
            }
        }
        bareFuncs_[name] = std::move(wrapped);
    }

    void registerFunction(const std::string &name, ExternalFunc func) override
    {
        if (name.empty() || !func) return;
        bareFuncs_[name] = wrapWithSelf(std::move(func));
    }

    Value callFunctionHandle(const Value &handle, Span<const Value> args, Environment * /*env*/) override
    {
        auto res = callFunctionHandleMulti(handle, args, 1, nullptr);
        return res.empty() ? Value() : std::move(res[0]);
    }

    std::vector<Value> callFunctionHandleMulti(const Value &handle,
                                               Span<const Value> args,
                                               size_t nargout,
                                               Environment * /*env*/) override
    {
        std::string fnName;
        if (handle.isFuncHandle()) {
            fnName = handle.funcHandleName();
        } else if (handle.isChar() || handle.isString()) {
            fnName = handle.toString();
        }
        if (fnName.empty() || fnName[0] == '@') {
            throw std::runtime_error(
                "Anonymous source closures require Full Engine (numkit_engine_create)");
        }
        const ExternalFunc *fn = findExternal(fnName, nullptr);
        if (!fn) {
            throw std::runtime_error("Undefined function handle '" + fnName + "'");
        }
        size_t actualOut = nargout == 0 ? 1 : nargout;
        std::vector<Value> outs(actualOut);
        CallContext ctx{reinterpret_cast<Engine *>(static_cast<PluginContext *>(this)), nullptr};
        (*fn)(args, nargout, Span<Value>(outs.data(), outs.size()), ctx);
        return outs;
    }

    ops::RngContext &rng() override
    {
        if (!rng_) {
            throw std::runtime_error("RngContext is not attached to this LitePluginContext");
        }
        return *rng_;
    }

    FsContext::ResolvedPath resolvePath(const std::string &userPath) const override
    {
        return fs_.resolvePath(userPath);
    }

    FigureManager &figureManager() override
    {
        throw std::runtime_error("FigureManager is not available in LitePluginContext");
    }

    const ExternalFunc *findExternal(const std::string &name,
                                     const Environment * /*env*/) const override
    {
        auto qIt = qualifiedFuncs_.find(name);
        if (qIt != qualifiedFuncs_.end()) return &qIt->second;
        auto bIt = bareFuncs_.find(name);
        if (bIt != bareFuncs_.end()) return &bIt->second;
        return nullptr;
    }

    void registerClass(BuiltinClass cls) override
    {
        std::string className = cls.name;
        if (cls.construct && bareFuncs_.find(className) == bareFuncs_.end()) {
            auto ctor = cls.construct;
            registerFunction(className, [ctor](Span<const Value> args,
                                               size_t /*nargout*/,
                                               Span<Value> outs,
                                               CallContext &ctx) {
                if (!outs.empty()) {
                    outs[0] = ctor(args, ctx);
                }
            });
        }
        classes_[className] = std::move(cls);
    }

    const BuiltinClass *findClass(const std::string &name) const override
    {
        auto it = classes_.find(name);
        return it != classes_.end() ? &it->second : nullptr;
    }

    void assignToCaller(int /*n*/, const std::string &name, Value val) override
    {
        callerVars_[name] = std::move(val);
    }

    const ExternalFunc *resolve(const std::string &ns, const std::string &name) const
    {
        if (!ns.empty()) {
            auto qIt = qualifiedFuncs_.find(ns + "." + name);
            if (qIt != qualifiedFuncs_.end()) return &qIt->second;
        }
        return findExternal(name, nullptr);
    }

    bool hasFunction(const std::string &ns, const std::string &name) const
    {
        return resolve(ns, name) != nullptr;
    }

    void invokeResolved(const ExternalFunc *fn,
                        Span<const Value> args,
                        size_t nargout,
                        Span<Value> outs)
    {
        if (!fn) {
            throw std::invalid_argument("Null pre-resolved function pointer");
        }
        CallContext ctx{reinterpret_cast<Engine *>(static_cast<PluginContext *>(this)), nullptr};
        (*fn)(args, nargout, outs, ctx);
    }

    void invoke(const std::string &ns,
                const std::string &name,
                Span<const Value> args,
                size_t nargout,
                Span<Value> outs)
    {
        CallContext ctx{reinterpret_cast<Engine *>(static_cast<PluginContext *>(this)), nullptr};

        if (!args.empty() && args[0].isObject()) {
            const BuiltinClass *cls = findClass(args[0].objectClassName());
            if (cls) {
                auto mIt = cls->methods.find(name);
                if (mIt != cls->methods.end() && mIt->second) {
                    Value selfCopy = args[0];
                    Span<const Value> rest(args.data() + 1, args.size() - 1);
                    mIt->second(selfCopy, rest, nargout, outs, ctx);
                    return;
                }
                auto opIt = cls->ops.find(name);
                if (opIt != cls->ops.end() && opIt->second) {
                    Value selfCopy = args[0];
                    opIt->second(selfCopy, args, nargout, outs, ctx);
                    return;
                }
            }
        }

        const ExternalFunc *fn = resolve(ns, name);
        if (!fn) {
            std::string full = ns.empty() ? name : (ns + "." + name);
            throw std::runtime_error("Undefined function '" + full + "'");
        }
        (*fn)(args, nargout, outs, ctx);
    }

    std::string formatValue(const Value &v) const
    {
        if (!v.isObject()) return v.toString();
        const BuiltinClass *cls = findClass(v.objectClassName());
        if (cls) {
            auto charIt = cls->methods.find("char");
            if (charIt != cls->methods.end() && charIt->second) {
                Value selfCopy = v;
                Value out;
                CallContext ctx{
                    reinterpret_cast<Engine *>(const_cast<LitePluginContext *>(this)), nullptr};
                charIt->second(selfCopy, Span<const Value>(), 1, Span<Value>(&out, 1), ctx);
                if (out.isChar() || out.isString()) {
                    return out.toString();
                }
            }
            if (cls->dispText) {
                std::string s = cls->dispText(v);
                while (!s.empty() && (s.front() == ' ' || s.front() == '\n')) s.erase(s.begin());
                while (!s.empty() && (s.back() == ' ' || s.back() == '\n')) s.pop_back();
                return s;
            }
        }
        return v.toString();
    }

private:
    ExternalFunc wrapWithSelf(ExternalFunc func)
    {
        return [this, fn = std::move(func)](Span<const Value> args,
                                            size_t nargout,
                                            Span<Value> outs,
                                            CallContext &ctx) {
            CallContext localCtx = ctx;
            localCtx.engine = reinterpret_cast<Engine *>(static_cast<PluginContext *>(this));
            fn(args, nargout, outs, localCtx);
        };
    }

    ops::RngContext *rng_;
    std::pmr::memory_resource *mr_;
    FsContext fs_;
    std::unordered_map<std::string, ExternalFunc> qualifiedFuncs_;
    std::unordered_map<std::string, ExternalFunc> bareFuncs_;
    std::unordered_map<std::string, BuiltinClass> classes_;
    std::unordered_map<std::string, Value> callerVars_;
};

} // namespace numkit::sys
