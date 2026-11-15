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
#include <numkit/value/value.hpp>

#include <keel/array.h>
#include <keel/format.h>
#include <keel/hash_map.h>
#include <keel/memory.h>
#include <keel/ref.h>
#include <keel/smart_ptr.h>
#include <keel/string.h>

#include <stdexcept>

namespace numkit::sys {

class LitePluginContext final : public PluginContext
{
public:
    explicit LitePluginContext(ops::RngContext *rng = nullptr,
                               keel::Allocator *alloc = keel::default_allocator())
        : rng_(rng)
        , alloc_(alloc ? alloc : keel::default_allocator())
        , qualifiedFuncs_(alloc_)
        , bareFuncs_(alloc_)
        , classes_(alloc_)
        , callerVars_(alloc_)
    {
    }

    ~LitePluginContext() override = default;

    LitePluginContext(const LitePluginContext &) = delete;
    LitePluginContext &operator=(const LitePluginContext &) = delete;
    LitePluginContext(LitePluginContext &&) = delete;
    LitePluginContext &operator=(LitePluginContext &&) = delete;

    void setRng(ops::RngContext *rng) noexcept { rng_ = rng; }

    keel::Allocator *allocator() const noexcept override { return alloc_; }

    void registerFunction(keel::StringRef ns, keel::StringRef name, ExternalFunc func) override
    {
        if (name.empty() || !func) return;
        auto wrapped = keel::make_arc_in<ExternalFunc>(alloc_, wrapWithSelf(keel::move(func)));
        if (!ns.empty()) {
            qualifiedFuncs_.put(keel::format_alloc(alloc_, "{}.{}", ns, name), wrapped);
            keel::usize dot = ns.find('.');
            if (dot != keel::kInvalidIndex) {
                keel::StringRef topNs = ns.substr(0, dot);
                if (!topNs.empty()) {
                    qualifiedFuncs_.put(keel::format_alloc(alloc_, "{}.{}", topNs, name), wrapped);
                }
            }
        }
        bareFuncs_.put(keel::String(name, alloc_), keel::move(wrapped));
    }

    void registerFunction(keel::StringRef name, ExternalFunc func) override
    {
        if (name.empty() || !func) return;
        auto wrapped = keel::make_arc_in<ExternalFunc>(alloc_, wrapWithSelf(keel::move(func)));
        bareFuncs_.put(keel::String(name, alloc_), keel::move(wrapped));
    }

    Value callFunctionHandle(const Value &handle, keel::ArrayRef<Value> args, Environment * /*env*/) override
    {
        auto res = callFunctionHandleMulti(handle, args, 1, nullptr);
        return res.empty() ? Value() : keel::move(res[0]);
    }

    keel::Array<Value> callFunctionHandleMulti(const Value &handle,
                                               keel::ArrayRef<Value> args,
                                               keel::usize nargout,
                                               Environment * /*env*/) override
    {
        keel::String fnName(alloc_);
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
            throw std::runtime_error(
                keel::format("Undefined function handle '{}'", fnName).c_str());
        }
        keel::usize actualOut = nargout == 0 ? 1 : nargout;
        keel::Array<Value> outs(alloc_);
        outs.resize(actualOut);
        CallContext ctx{reinterpret_cast<Engine *>(static_cast<PluginContext *>(this)), nullptr};
        (*fn)(args, nargout, keel::MutArrayRef<Value>(outs.data(), outs.size()), ctx);
        return outs;
    }

    ops::RngContext &rng() override
    {
        if (!rng_) {
            throw std::runtime_error("RngContext is not attached to this LitePluginContext");
        }
        return *rng_;
    }

    FsContext::ResolvedPath resolvePath(keel::StringRef userPath) const override
    {
        return fs_.resolvePath(userPath);
    }

    FigureManager &figureManager() override
    {
        throw std::runtime_error("FigureManager is not available in LitePluginContext");
    }

    const ExternalFunc *findExternal(keel::StringRef name,
                                     const Environment * /*env*/) const override
    {
        if (const auto *q = qualifiedFuncs_.get(name)) return q->get();
        if (const auto *b = bareFuncs_.get(name)) return b->get();
        return nullptr;
    }

    void registerClass(BuiltinClass cls) override
    {
        keel::String className = cls.name.clone(alloc_);
        const bool hasCtor = static_cast<bool>(cls.construct);
        classes_.put(className.clone(alloc_), keel::make_unique_in<BuiltinClass>(alloc_, keel::move(cls)));
        if (hasCtor && !bareFuncs_.contains(className)) {
            keel::StringRef nameRef = className;
            registerFunction(nameRef, [this, cname = keel::move(className)](keel::ArrayRef<Value> args,
                                                                            keel::usize /*nargout*/,
                                                                            keel::MutArrayRef<Value> outs,
                                                                            CallContext &ctx) {
                const BuiltinClass *c = this->findClass(cname);
                if (c && c->construct && !outs.empty()) {
                    outs[0] = c->construct(args, ctx);
                }
            });
        }
    }

    const BuiltinClass *findClass(keel::StringRef name) const override
    {
        const auto *it = classes_.get(name);
        return it ? it->get() : nullptr;
    }

    void assignToCaller(keel::i32 /*n*/, keel::StringRef name, Value val) override
    {
        callerVars_.put(keel::String(name, alloc_), keel::move(val));
    }

    const ExternalFunc *resolve(keel::StringRef ns, keel::StringRef name) const
    {
        if (!ns.empty()) {
            keel::SmallString<128> key;
            (void)keel::format_into(key, "{}.{}", ns, name);
            if (const auto *q = qualifiedFuncs_.get(key.as_ref())) return q->get();
        }
        return findExternal(name, nullptr);
    }

    bool hasFunction(keel::StringRef ns, keel::StringRef name) const
    {
        return resolve(ns, name) != nullptr;
    }

    void invokeResolved(const ExternalFunc *fn,
                        keel::ArrayRef<Value> args,
                        keel::usize nargout,
                        keel::MutArrayRef<Value> outs)
    {
        if (!fn) {
            throw std::invalid_argument("Null pre-resolved function pointer");
        }
        CallContext ctx{reinterpret_cast<Engine *>(static_cast<PluginContext *>(this)), nullptr};
        (*fn)(args, nargout, outs, ctx);
    }

    void invoke(keel::StringRef ns,
                keel::StringRef name,
                keel::ArrayRef<Value> args,
                keel::usize nargout,
                keel::MutArrayRef<Value> outs)
    {
        CallContext ctx{reinterpret_cast<Engine *>(static_cast<PluginContext *>(this)), nullptr};

        if (!args.empty() && args[0].isObject()) {
            const BuiltinClass *cls = findClass(args[0].objectClassName());
            if (cls) {
                if (const ObjectMethod *m = cls->methods.get(name); m && *m) {
                    Value selfCopy = args[0];
                    keel::ArrayRef<Value> rest = args.slice(1);
                    (*m)(selfCopy, rest, nargout, outs, ctx);
                    return;
                }
                if (const ObjectMethod *op = cls->ops.get(name); op && *op) {
                    Value selfCopy = args[0];
                    (*op)(selfCopy, args, nargout, outs, ctx);
                    return;
                }
            }
        }

        const ExternalFunc *fn = resolve(ns, name);
        if (!fn) {
            if (ns.empty()) {
                throw std::runtime_error(keel::format("Undefined function '{}'", name).c_str());
            }
            throw std::runtime_error(keel::format("Undefined function '{}.{}'", ns, name).c_str());
        }
        (*fn)(args, nargout, outs, ctx);
    }

    keel::String formatValue(const Value &v) const
    {
        if (!v.isObject()) return v.toString();
        const BuiltinClass *cls = findClass(v.objectClassName());
        if (cls) {
            if (const ObjectMethod *charFn = cls->methods.get("char"); charFn && *charFn) {
                Value selfCopy = v;
                Value out;
                CallContext ctx{
                    reinterpret_cast<Engine *>(const_cast<LitePluginContext *>(this)), nullptr};
                (*charFn)(selfCopy, keel::ArrayRef<Value>(), 1, keel::MutArrayRef<Value>(&out, 1), ctx);
                if (out.isChar() || out.isString()) {
                    return out.toString();
                }
            }
            if (cls->dispText) {
                keel::String s = cls->dispText(v);
                keel::StringRef r = s.as_ref();
                while (!r.empty() && (r.front() == ' ' || r.front() == '\n'))
                    r = r.substr(1);
                while (!r.empty() && (r.back() == ' ' || r.back() == '\n'))
                    r = r.substr(0, r.size() - 1);
                return keel::String(r, alloc_);
            }
        }
        return v.toString();
    }

private:
    ExternalFunc wrapWithSelf(ExternalFunc func)
    {
        return ExternalFunc([this, fn = keel::move(func)](keel::ArrayRef<Value> args,
                                                          keel::usize nargout,
                                                          keel::MutArrayRef<Value> outs,
                                                          CallContext &ctx) {
            CallContext localCtx = ctx;
            localCtx.engine = reinterpret_cast<Engine *>(static_cast<PluginContext *>(this));
            fn(args, nargout, outs, localCtx);
        }, alloc_);
    }

    ops::RngContext *rng_;
    keel::Allocator *alloc_;
    FsContext fs_;
    keel::HashMap<keel::String, keel::Arc<ExternalFunc>> qualifiedFuncs_;
    keel::HashMap<keel::String, keel::Arc<ExternalFunc>> bareFuncs_;
    keel::HashMap<keel::String, keel::UniquePtr<BuiltinClass>> classes_;
    keel::HashMap<keel::String, Value> callerVars_;
};

} // namespace numkit::sys
