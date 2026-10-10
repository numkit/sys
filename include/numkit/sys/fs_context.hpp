// include/numkit/sys/fs_context.hpp
//
// FsContext — the engine-agnostic filesystem session: the VirtualFS registry,
// the script-origin stack, the current working directory, and the path
// resolver that ties them together.
#pragma once

#include <numkit/sys/branding.hpp>
#include <numkit/sys/vfs.hpp>

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace numkit::sys {

class FsContext
{
public:
    // ── Virtual filesystem registry ───────────────────────────
    void registerVirtualFS(std::unique_ptr<VirtualFS> fs);
    VirtualFS *findVirtualFS(const std::string &name) const;

    // ── Script-origin stack ───────────────────────────────────
    void pushScriptOrigin(const std::string &fsName);
    void pushScriptOrigin(const std::string &fsName, const std::string &scriptDir);
    void pushScriptOrigin(const std::string &fsName, const std::string &scriptDir,
                          const std::string &scriptFile);
    void popScriptOrigin();
    const std::string *currentScriptOrigin() const;
    const std::string *currentScriptDir() const;
    const std::string *currentScriptFile() const;

    size_t scriptOriginDepth() const { return scriptOriginStack_.size(); }
    void truncateScriptOrigins(size_t depth)
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
        size_t depth_;
    };

    // ── Current working directory ─────────────────────────────
    const std::string &cwd() const { return cwd_; }
    void setCwd(const std::string &p) { cwd_ = p; }

    // ── Path resolution ───────────────────────────────────────
    struct ResolvedPath
    {
        VirtualFS *fs = nullptr;
        std::string path;
    };
    ResolvedPath resolvePath(const std::string &userPath) const;

    // ── MATLAB-style file descriptor table ────────────────────
    struct OpenFile
    {
        std::string path;
        std::string mode;
        VirtualFS *fs = nullptr;
        std::string buffer;
        std::size_t cursor = 0;
        bool forRead = false;
        bool forWrite = false;
        bool appendOnly = false;
        std::string lastError;
    };

    int openFile(const std::string &userPath, const std::string &mode);
    bool closeFile(int fid);
    void closeAllFiles();
    OpenFile *findFile(int fid);
    std::vector<int> openFileIds() const;
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

    std::unordered_map<int, OpenFile> openFiles_;
    int nextFid_ = 3;
    std::string lastFopenError_;
};

} // namespace numkit::sys

namespace numkit {
    using sys::FsContext;
} // namespace numkit
