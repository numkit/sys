// include/numkit/sys/fs_context.hpp
//
// FsContext — the engine-agnostic filesystem session: the VirtualFS registry,
// the script-origin stack, the current working directory, and the path
// resolver that ties them together.
#pragma once

#include <numkit/sys/env.hpp>
#include <numkit/sys/vfs.hpp>

#include <keel/core.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace numkit::sys {

class FsContext
{
public:
    void registerVirtualFS(std::unique_ptr<VirtualFS> fs);
    VirtualFS *findVirtualFS(const std::string &name) const;

    void pushScriptOrigin(const std::string &fsName);
    void pushScriptOrigin(const std::string &fsName, const std::string &scriptDir);
    void pushScriptOrigin(const std::string &fsName, const std::string &scriptDir,
                          const std::string &scriptFile);
    void popScriptOrigin();
    const std::string *currentScriptOrigin() const;
    const std::string *currentScriptDir() const;
    const std::string *currentScriptFile() const;

    keel::usize scriptOriginDepth() const { return scriptOriginStack_.size(); }
    void truncateScriptOrigins(keel::usize depth)
    {
        if (depth < scriptOriginStack_.size())
            scriptOriginStack_.resize(depth);
    }

    class OriginFrame
    {
    public:
        OriginFrame(FsContext &fs, const std::string &fsName,
                    const std::string &scriptDir, const std::string &scriptFile)
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

    const std::string &cwd() const { return cwd_; }
    void setCwd(const std::string &p) { cwd_ = p; }

    struct ResolvedPath
    {
        VirtualFS *fs = nullptr;
        std::string path;
    };
    ResolvedPath resolvePath(const std::string &userPath) const;

    struct OpenFile
    {
        std::string path;
        std::string mode;
        VirtualFS *fs = nullptr;
        std::string buffer;
        keel::usize cursor = 0;
        bool forRead = false;
        bool forWrite = false;
        bool appendOnly = false;
        std::string lastError;
    };

    keel::i32 openFile(const std::string &userPath, const std::string &mode);
    bool closeFile(keel::i32 fid);
    void closeAllFiles();
    OpenFile *findFile(keel::i32 fid);
    std::vector<keel::i32> openFileIds() const;
    const std::string &lastFopenError() const { return lastFopenError_; }

private:
    std::unordered_map<std::string, std::unique_ptr<VirtualFS>> virtualFs_;
    struct ScriptOriginEntry
    {
        std::string fsName;
        std::string scriptDir;
        std::string scriptFile;
    };
    std::vector<ScriptOriginEntry> scriptOriginStack_;
    std::string cwd_;

    std::unordered_map<keel::i32, OpenFile> openFiles_;
    std::string lastFopenError_;
};

} // namespace numkit::sys

namespace numkit {
    using sys::FsContext;
} // namespace numkit
