// src/fs_context.cpp
//
// FsContext — VirtualFS registry, script-origin stack, cwd, and path resolver.

#include <numkit/sys/fs_context.hpp>

#include <keel/algo.h>
#include <keel/format.h>

#include <stdexcept>

namespace numkit::sys {

// ── Registry ──────────────────────────────────────────────────

void FsContext::registerVirtualFS(keel::UniquePtr<VirtualFS> fs)
{
    if (!fs)
        return;
    keel::String n = fs->name();
    virtualFs_.put(keel::move(n), keel::move(fs));
}

VirtualFS *FsContext::findVirtualFS(keel::StringRef name) const
{
    const auto *it = virtualFs_.get(name);
    return it ? it->get() : nullptr;
}

// ── Script-origin stack ───────────────────────────────────────

void FsContext::pushScriptOrigin(keel::StringRef fsName)
{
    scriptOriginStack_.push_back({keel::String(fsName), keel::String{}, keel::String{}});
}

void FsContext::pushScriptOrigin(keel::StringRef fsName, keel::StringRef scriptDir)
{
    scriptOriginStack_.push_back({keel::String(fsName), keel::String(scriptDir), keel::String{}});
}

void FsContext::pushScriptOrigin(keel::StringRef fsName, keel::StringRef scriptDir,
                                 keel::StringRef scriptFile)
{
    scriptOriginStack_.push_back(
        {keel::String(fsName), keel::String(scriptDir), keel::String(scriptFile)});
}

void FsContext::popScriptOrigin()
{
    if (!scriptOriginStack_.empty())
        scriptOriginStack_.pop_back();
}

const keel::String *FsContext::currentScriptOrigin() const
{
    return scriptOriginStack_.empty() ? nullptr : &scriptOriginStack_.back().fsName;
}

const keel::String *FsContext::currentScriptDir() const
{
    return scriptOriginStack_.empty() ? nullptr : &scriptOriginStack_.back().scriptDir;
}

const keel::String *FsContext::currentScriptFile() const
{
    return scriptOriginStack_.empty() ? nullptr : &scriptOriginStack_.back().scriptFile;
}

// ── Path resolution ───────────────────────────────────────────

namespace {

struct FsSchemeSplit
{
    keel::StringRef scheme;
    keel::StringRef rest;
};

FsSchemeSplit splitFsScheme(
    keel::StringRef path,
    const keel::HashMap<keel::String, keel::UniquePtr<VirtualFS>> &fsMap)
{
    keel::usize colon = path.find(':');
    if (colon == keel::kInvalidIndex || colon < 2)
        return {keel::StringRef{}, path};
    keel::StringRef scheme = path.substr(0, colon);
    if (!fsMap.contains(scheme))
        return {keel::StringRef{}, path};
    return {scheme, path.substr(colon + 1)};
}

bool isAbsolutePath(keel::StringRef p) noexcept
{
    if (p.empty())
        return false;
    if (p[0] == '/' || p[0] == '\\')
        return true;
    if (p.size() >= 2 && p[1] == ':' &&
        ((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')))
        return true;
    return false;
}

keel::String joinPath(keel::StringRef base, keel::StringRef rel)
{
    if (base.empty())
        return keel::String(rel);
    if (rel.empty())
        return keel::String(base);
    char last = base.back();
    if (last == '/' || last == '\\')
        return keel::format("{}{}", base, rel);
    return keel::format("{}/{}", base, rel);
}

} // namespace

FsContext::ResolvedPath FsContext::resolvePath(keel::StringRef userPath) const
{
    // 1. Explicit scheme in the path wins.
    auto split = splitFsScheme(userPath, virtualFs_);
    if (!split.scheme.empty()) {
        auto *fs = findVirtualFS(split.scheme);
        if (!fs)
            throw std::runtime_error(
                keel::format("unknown filesystem '{}' in path", split.scheme).c_str());
        return {fs, keel::String(split.rest)};
    }

    // 2. NUMKIT_FS env var selects the backend.
    keel::String fsName = envGetPrefixed("FS");
    if (fsName == "auto")
        fsName.clear();

    // 3. Fall back to script origin, then to "native".
    if (fsName.empty()) {
        if (auto *o = currentScriptOrigin())
            fsName = o->clone();
    }
    if (fsName.empty())
        fsName = keel::String::from("native");

    VirtualFS *fs = findVirtualFS(fsName);
    if (!fs)
        throw std::runtime_error(
            keel::format("filesystem '{}' is not available", fsName).c_str());

    keel::String path(userPath);
    if (!isAbsolutePath(path)) {
        keel::String cwdVal = !cwd_.empty() ? cwd_.clone() : envGetPrefixed("CWD");
        if (cwdVal.empty()) {
            if (auto *sdir = currentScriptDir()) {
                if (!sdir->empty()) cwdVal = sdir->clone();
            }
        }
        if (!cwdVal.empty())
            path = joinPath(cwdVal, path);
    }

    return {fs, keel::move(path)};
}

// ── Engine file descriptor table ──────────────────────────────

keel::i32 FsContext::openFile(keel::StringRef userPath, keel::StringRef modeRaw)
{
    lastFopenError_.clear();

    keel::String modeNorm;
    modeNorm.reserve(modeRaw.size());
    for (keel::usize i = 0; i < modeRaw.size(); ++i) {
        const char ch = modeRaw[i];
        if (i > 0 && (ch == 'b' || ch == 't'))
            continue;
        modeNorm.push_back(ch);
    }
    const keel::StringRef modeView = modeNorm.as_ref();

    bool canRead = false, canWrite = false, appendOnly = false, truncate = false, seedBuffer = false;
    if      (modeView == "r")  { canRead = true;  seedBuffer = true; }
    else if (modeView.equals_ignore_ascii_case("w")) { canWrite = true; truncate = true; }
    else if (modeView.equals_ignore_ascii_case("a")) { canWrite = true; appendOnly = true; seedBuffer = true; }
    else if (modeView == "r+") { canRead = true;  canWrite = true; seedBuffer = true; }
    else if (modeView == "w+") { canRead = true;  canWrite = true; truncate = true; }
    else if (modeView == "a+") { canRead = true;  canWrite = true; appendOnly = true; seedBuffer = true; }
    else {
        lastFopenError_ = keel::String::from("Invalid permission specified");
        return -1;
    }

    ResolvedPath r;
    try {
        r = resolvePath(userPath);
    } catch (const std::exception &e) {
        lastFopenError_ = keel::String(e.what());
        return -1;
    }

    OpenFile f;
    f.path = r.path.clone();
    f.mode = keel::String(modeView);
    f.fs = r.fs;
    f.forRead = canRead;
    f.forWrite = canWrite;
    f.appendOnly = appendOnly;

    const bool useNativeOsCwd = !isAbsolutePath(userPath)
        && cwd_.empty() && envGetPrefixed("CWD").empty()
        && r.fs == findVirtualFS("native");
    if (useNativeOsCwd && canWrite && modeView != "r+") {
        f.path = keel::String(userPath);
    }

    if (seedBuffer) {
        const bool requireExisting = (modeView == "r" || modeView == "r+");
        try {
            if (useNativeOsCwd && r.fs->exists(userPath)) {
                f.path = keel::String(userPath);
                f.buffer = r.fs->readFile(userPath);
            } else if (r.fs->exists(f.path)) {
                f.buffer = r.fs->readFile(f.path);
            } else if (requireExisting && !isAbsolutePath(userPath) && r.fs->exists(userPath)) {
                f.path = keel::String(userPath);
                f.buffer = r.fs->readFile(userPath);
            } else if (requireExisting) {
                lastFopenError_ = keel::String::from("No such file or directory");
                return -1;
            }
        } catch (const std::exception &e) {
            if (requireExisting) {
                lastFopenError_ = keel::String(e.what());
                return -1;
            }
            f.buffer.clear();
        }
    }
    if (truncate)
        f.buffer.clear();
    if (appendOnly)
        f.cursor = f.buffer.size();

    keel::i32 fid = 3;
    while (openFiles_.contains(fid))
        ++fid;
    openFiles_.put(fid, keel::move(f));
    return fid;
}

bool FsContext::closeFile(keel::i32 fid)
{
    auto *f = openFiles_.get(fid);
    if (!f)
        return false;

    bool ok = true;
    if (f->forWrite) {
        try {
            f->fs->writeFile(f->path, f->buffer);
        } catch (const std::exception &) {
            ok = false;
        }
    }
    openFiles_.remove(fid);
    return ok;
}

void FsContext::closeAllFiles()
{
    keel::Array<keel::i32> fids;
    fids.reserve(openFiles_.size());
    for (auto entry : openFiles_)
        fids.push_back(entry.key);
    for (keel::i32 fid : fids)
        closeFile(fid);
}

FsContext::OpenFile *FsContext::findFile(keel::i32 fid)
{
    return openFiles_.get(fid);
}

keel::Array<keel::i32> FsContext::openFileIds() const
{
    keel::Array<keel::i32> ids;
    ids.reserve(openFiles_.size());
    for (auto entry : openFiles_)
        ids.push_back(entry.key);
    keel::sort(ids);
    return ids;
}

} // namespace numkit::sys
