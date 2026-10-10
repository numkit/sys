// include/numkit/sys/vfs.hpp
//
// Unified filesystem abstraction for NumKit.
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace numkit::sys {

// ── DirEntry / FileStat — types returned by VFS introspection methods ──

struct DirEntry
{
    std::string name;        // basename only (no path prefix)
    bool isDirectory = false;
};

struct FileStat
{
    int64_t size = 0;        // bytes (0 for directories)
    int64_t mtime = 0;       // last-modified time, seconds since epoch
                             // (0 if backend can't supply one)
    enum class Kind { File, Directory, Other };
    Kind kind = Kind::Other;
};

class VirtualFS
{
public:
    virtual ~VirtualFS() = default;

    virtual std::string readFile(const std::string &path) = 0;
    virtual void writeFile(const std::string &path, const std::string &content) = 0;
    virtual bool exists(const std::string &path) = 0;

    // Binary-safe read — returns raw file bytes without UTF-8 reinterpretation
    virtual std::string readFileBytes(const std::string &path) { return readFile(path); }

    // Binary-safe write — verbatim byte container without encoding translation
    virtual void writeFileBytes(const std::string &path, const std::string &bytes)
    {
        writeFile(path, bytes);
    }

    virtual std::string name() const = 0;

    // Directory operations and introspection
    virtual std::vector<DirEntry> listDir(const std::string &/*path*/)
    {
        throw std::runtime_error(name() + ": listDir not supported");
    }

    virtual std::optional<FileStat> stat(const std::string &/*path*/)
    {
        return std::nullopt;
    }

    virtual void mkdir(const std::string &/*path*/)
    {
        throw std::runtime_error(name() + ": mkdir not supported");
    }

    virtual void rmdir(const std::string &/*path*/)
    {
        throw std::runtime_error(name() + ": rmdir not supported");
    }

    virtual void unlink(const std::string &/*path*/)
    {
        throw std::runtime_error(name() + ": unlink not supported");
    }

    virtual std::string tempArea()
    {
        return {};
    }

    virtual std::string cwd()
    {
        return {};
    }
};

// ── NativeFS — std::filesystem / std::ifstream / std::ofstream ──
class NativeFS final : public VirtualFS
{
public:
    std::string readFile(const std::string &path) override;
    void writeFile(const std::string &path, const std::string &content) override;
    bool exists(const std::string &path) override;
    std::string name() const override { return "native"; }

    std::vector<DirEntry> listDir(const std::string &path) override;
    std::optional<FileStat> stat(const std::string &path) override;
    void mkdir(const std::string &path) override;
    void rmdir(const std::string &path) override;
    void unlink(const std::string &path) override;
    std::string tempArea() override;
    std::string cwd() override;
};

// ── CallbackFS — delegates to std::function hooks (WASM / browser bridge) ──
class CallbackFS final : public VirtualFS
{
public:
    using ReadFunc = std::function<std::string(const std::string &)>;
    using WriteFunc = std::function<void(const std::string &, const std::string &)>;
    using ExistsFunc = std::function<bool(const std::string &)>;
    using ReadBytesFunc = std::function<std::string(const std::string &)>;
    using WriteBytesFunc = std::function<void(const std::string &, const std::string &)>;

    using ListDirFunc = std::function<std::vector<DirEntry>(const std::string &)>;
    using StatFunc = std::function<std::optional<FileStat>(const std::string &)>;
    using MkdirFunc = std::function<void(const std::string &)>;
    using RmdirFunc = std::function<void(const std::string &)>;
    using UnlinkFunc = std::function<void(const std::string &)>;
    using TempAreaFunc = std::function<std::string()>;
    using CwdFunc = std::function<std::string()>;

    CallbackFS(std::string n, ReadFunc r, WriteFunc w, ExistsFunc e)
        : name_(std::move(n)), read_(std::move(r)), write_(std::move(w)), exists_(std::move(e))
    {}

    std::string readFile(const std::string &path) override
    {
        if (!read_)
            throw std::runtime_error(name_ + ": read hook not installed");
        return read_(path);
    }
    void writeFile(const std::string &path, const std::string &content) override
    {
        if (!write_)
            throw std::runtime_error(name_ + ": write hook not installed");
        write_(path, content);
    }
    bool exists(const std::string &path) override { return exists_ ? exists_(path) : false; }
    std::string name() const override { return name_; }

    void setReadBytes(ReadBytesFunc f) { readBytes_ = std::move(f); }
    std::string readFileBytes(const std::string &path) override
    {
        if (readBytes_) return readBytes_(path);
        return readFile(path);
    }

    void setWriteBytes(WriteBytesFunc f) { writeBytes_ = std::move(f); }
    void writeFileBytes(const std::string &path, const std::string &bytes) override
    {
        if (writeBytes_) { writeBytes_(path, bytes); return; }
        writeFile(path, bytes);
    }

    void setListDir(ListDirFunc f) { listDir_ = std::move(f); }
    void setStat(StatFunc f) { stat_ = std::move(f); }
    void setMkdir(MkdirFunc f) { mkdir_ = std::move(f); }
    void setRmdir(RmdirFunc f) { rmdir_ = std::move(f); }
    void setUnlink(UnlinkFunc f) { unlink_ = std::move(f); }
    void setTempArea(TempAreaFunc f) { tempArea_ = std::move(f); }
    void setCwd(CwdFunc f) { cwd_ = std::move(f); }

    std::vector<DirEntry> listDir(const std::string &path) override
    {
        if (listDir_) return listDir_(path);
        return VirtualFS::listDir(path);
    }
    std::optional<FileStat> stat(const std::string &path) override
    {
        if (stat_) return stat_(path);
        return std::nullopt;
    }
    void mkdir(const std::string &path) override
    {
        if (mkdir_) { mkdir_(path); return; }
        VirtualFS::mkdir(path);
    }
    void rmdir(const std::string &path) override
    {
        if (rmdir_) { rmdir_(path); return; }
        VirtualFS::rmdir(path);
    }
    void unlink(const std::string &path) override
    {
        if (unlink_) { unlink_(path); return; }
        VirtualFS::unlink(path);
    }
    std::string tempArea() override
    {
        return tempArea_ ? tempArea_() : std::string{};
    }
    std::string cwd() override
    {
        return cwd_ ? cwd_() : std::string{};
    }

private:
    std::string name_;
    ReadFunc read_;
    WriteFunc write_;
    ExistsFunc exists_;
    ReadBytesFunc readBytes_;
    WriteBytesFunc writeBytes_;
    ListDirFunc listDir_;
    StatFunc stat_;
    MkdirFunc mkdir_;
    RmdirFunc rmdir_;
    UnlinkFunc unlink_;
    TempAreaFunc tempArea_;
    CwdFunc cwd_;
};

} // namespace numkit::sys

namespace numkit {
    using sys::DirEntry;
    using sys::FileStat;
    using sys::VirtualFS;
    using sys::NativeFS;
    using sys::CallbackFS;
} // namespace numkit
