// include/numkit/sys/fs_context.hpp
//
// FsContext — the engine-agnostic filesystem session: the VirtualFS registry,
// the script-origin stack, the current working directory, and the path
// resolver that ties them together.
#pragma once

#include <numkit/sys/env.hpp>
#include <numkit/sys/vfs.hpp>

#include <keel/array.h>
#include <keel/core.h>
#include <keel/hash_map.h>
#include <keel/smart_ptr.h>
#include <keel/string.h>

namespace numkit::sys {

class FsContext
{
public:
    void registerVirtualFS(keel::UniquePtr<VirtualFS> fs);
    VirtualFS *findVirtualFS(keel::StringRef name) const;

    void pushScriptOrigin(keel::StringRef fsName);
    void pushScriptOrigin(keel::StringRef fsName, keel::StringRef scriptDir);
    void pushScriptOrigin(keel::StringRef fsName, keel::StringRef scriptDir,
                          keel::StringRef scriptFile);
    void popScriptOrigin();
    const keel::String *currentScriptOrigin() const;
    const keel::String *currentScriptDir() const;
    const keel::String *currentScriptFile() const;

    keel::usize scriptOriginDepth() const noexcept { return scriptOriginStack_.size(); }
    void truncateScriptOrigins(keel::usize depth)
    {
        if (depth < scriptOriginStack_.size())
            scriptOriginStack_.resize(depth);
    }

    class OriginFrame
    {
    public:
        OriginFrame(FsContext &fs, keel::StringRef fsName,
                    keel::StringRef scriptDir, keel::StringRef scriptFile)
            : fs_(fs)
            , depth_(fs.scriptOriginDepth())
        {
            fs_.pushScriptOrigin(fsName, scriptDir, scriptFile);
        }
        ~OriginFrame() { fs_.truncateScriptOrigins(depth_); }
        OriginFrame(const OriginFrame &) = delete;
        OriginFrame &operator=(const OriginFrame &) = delete;

    private:
        FsContext &fs_;
        keel::usize depth_;
    };

    const keel::String &cwd() const noexcept { return cwd_; }
    void setCwd(keel::StringRef p) { cwd_.assign(p); }

    struct ResolvedPath
    {
        VirtualFS *fs = nullptr;
        keel::String path;
    };
    ResolvedPath resolvePath(keel::StringRef userPath) const;

    struct OpenFile
    {
        keel::String path;
        keel::String mode;
        VirtualFS *fs = nullptr;
        keel::String buffer;
        keel::usize cursor = 0;
        bool forRead = false;
        bool forWrite = false;
        bool appendOnly = false;
        keel::String lastError;
    };

    keel::i32 openFile(keel::StringRef userPath, keel::StringRef mode);
    bool closeFile(keel::i32 fid);
    void closeAllFiles();
    OpenFile *findFile(keel::i32 fid);
    keel::Array<keel::i32> openFileIds() const;
    const keel::String &lastFopenError() const noexcept { return lastFopenError_; }

private:
    keel::HashMap<keel::String, keel::UniquePtr<VirtualFS>> virtualFs_;
    struct ScriptOriginEntry
    {
        keel::String fsName;
        keel::String scriptDir;
        keel::String scriptFile;
    };
    keel::Array<ScriptOriginEntry> scriptOriginStack_;
    keel::String cwd_;

    keel::HashMap<keel::i32, OpenFile> openFiles_;
    keel::String lastFopenError_;
};

} // namespace numkit::sys

namespace numkit {
    using sys::FsContext;
} // namespace numkit
