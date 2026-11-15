// include/numkit/sys/vfs.hpp
//
// Unified filesystem abstraction for NumKit.
#pragma once

#include <keel/array.h>
#include <keel/core.h>
#include <keel/format.h>
#include <keel/function.h>
#include <keel/optional.h>
#include <keel/string.h>

#include <stdexcept>

namespace numkit::sys {

struct DirEntry
{
    keel::String name;       // basename only (no path prefix)
    bool isDirectory = false;
};

struct FileStat
{
    keel::i64 size = 0;      // bytes (0 for directories)
    keel::i64 mtime = 0;     // last-modified time, seconds since epoch
    keel::i64 mtimeNs = 0;   // high-resolution mtime in nanoseconds since epoch
    enum class Kind { File, Directory, Other };
    Kind kind = Kind::Other;
};

class VirtualFS
{
public:
    virtual ~VirtualFS() = default;

    virtual keel::String readFile(keel::StringRef path) = 0;
    virtual void writeFile(keel::StringRef path, keel::StringRef content) = 0;
    virtual bool exists(keel::StringRef path) = 0;

    virtual keel::String readFileBytes(keel::StringRef path) { return readFile(path); }

    virtual void writeFileBytes(keel::StringRef path, keel::StringRef bytes)
    {
        writeFile(path, bytes);
    }

    virtual keel::String name() const = 0;

    virtual keel::Array<DirEntry> listDir(keel::StringRef /*path*/)
    {
        throw std::runtime_error(keel::format("{}: listDir not supported", name()).c_str());
    }

    virtual keel::Optional<FileStat> stat(keel::StringRef /*path*/)
    {
        return keel::kNullOpt;
    }

    virtual void mkdir(keel::StringRef /*path*/)
    {
        throw std::runtime_error(keel::format("{}: mkdir not supported", name()).c_str());
    }

    virtual void rmdir(keel::StringRef /*path*/)
    {
        throw std::runtime_error(keel::format("{}: rmdir not supported", name()).c_str());
    }

    virtual void unlink(keel::StringRef /*path*/)
    {
        throw std::runtime_error(keel::format("{}: unlink not supported", name()).c_str());
    }

    virtual keel::String tempArea()
    {
        return {};
    }

    virtual keel::String cwd()
    {
        return {};
    }
};

// ── NativeFS — backed by keel::fs_* ──
class NativeFS final : public VirtualFS
{
public:
    keel::String readFile(keel::StringRef path) override;
    void writeFile(keel::StringRef path, keel::StringRef content) override;
    bool exists(keel::StringRef path) override;
    keel::String name() const override { return keel::String::from("native"); }

    keel::Array<DirEntry> listDir(keel::StringRef path) override;
    keel::Optional<FileStat> stat(keel::StringRef path) override;
    void mkdir(keel::StringRef path) override;
    void rmdir(keel::StringRef path) override;
    void unlink(keel::StringRef path) override;
    keel::String tempArea() override;
    keel::String cwd() override;
};

// ── CallbackFS — delegates to keel::Function hooks (WASM / browser bridge) ──
class CallbackFS final : public VirtualFS
{
public:
    using ReadFunc = keel::Function<keel::String(keel::StringRef)>;
    using WriteFunc = keel::Function<void(keel::StringRef, keel::StringRef)>;
    using ExistsFunc = keel::Function<bool(keel::StringRef)>;
    using ReadBytesFunc = keel::Function<keel::String(keel::StringRef)>;
    using WriteBytesFunc = keel::Function<void(keel::StringRef, keel::StringRef)>;

    using ListDirFunc = keel::Function<keel::Array<DirEntry>(keel::StringRef)>;
    using StatFunc = keel::Function<keel::Optional<FileStat>(keel::StringRef)>;
    using MkdirFunc = keel::Function<void(keel::StringRef)>;
    using RmdirFunc = keel::Function<void(keel::StringRef)>;
    using UnlinkFunc = keel::Function<void(keel::StringRef)>;
    using TempAreaFunc = keel::Function<keel::String()>;
    using CwdFunc = keel::Function<keel::String()>;

    CallbackFS(keel::StringRef n, ReadFunc r, WriteFunc w, ExistsFunc e)
        : name_(n)
        , read_(keel::move(r))
        , write_(keel::move(w))
        , exists_(keel::move(e))
    {}

    keel::String readFile(keel::StringRef path) override
    {
        if (!read_)
            throw std::runtime_error(keel::format("{}: read hook not installed", name_).c_str());
        return read_(path);
    }
    void writeFile(keel::StringRef path, keel::StringRef content) override
    {
        if (!write_)
            throw std::runtime_error(keel::format("{}: write hook not installed", name_).c_str());
        write_(path, content);
    }
    bool exists(keel::StringRef path) override { return exists_ ? exists_(path) : false; }
    keel::String name() const override { return name_.clone(); }

    void setReadBytes(ReadBytesFunc f) { readBytes_ = keel::move(f); }
    keel::String readFileBytes(keel::StringRef path) override
    {
        if (readBytes_) return readBytes_(path);
        return readFile(path);
    }

    void setWriteBytes(WriteBytesFunc f) { writeBytes_ = keel::move(f); }
    void writeFileBytes(keel::StringRef path, keel::StringRef bytes) override
    {
        if (writeBytes_) { writeBytes_(path, bytes); return; }
        writeFile(path, bytes);
    }

    void setListDir(ListDirFunc f) { listDir_ = keel::move(f); }
    void setStat(StatFunc f) { stat_ = keel::move(f); }
    void setMkdir(MkdirFunc f) { mkdir_ = keel::move(f); }
    void setRmdir(RmdirFunc f) { rmdir_ = keel::move(f); }
    void setUnlink(UnlinkFunc f) { unlink_ = keel::move(f); }
    void setTempArea(TempAreaFunc f) { tempArea_ = keel::move(f); }
    void setCwd(CwdFunc f) { cwd_ = keel::move(f); }

    keel::Array<DirEntry> listDir(keel::StringRef path) override
    {
        if (listDir_) return listDir_(path);
        return VirtualFS::listDir(path);
    }
    keel::Optional<FileStat> stat(keel::StringRef path) override
    {
        if (stat_) return stat_(path);
        return keel::kNullOpt;
    }
    void mkdir(keel::StringRef path) override
    {
        if (mkdir_) { mkdir_(path); return; }
        VirtualFS::mkdir(path);
    }
    void rmdir(keel::StringRef path) override
    {
        if (rmdir_) { rmdir_(path); return; }
        VirtualFS::rmdir(path);
    }
    void unlink(keel::StringRef path) override
    {
        if (unlink_) { unlink_(path); return; }
        VirtualFS::unlink(path);
    }
    keel::String tempArea() override
    {
        return tempArea_ ? tempArea_() : keel::String{};
    }
    keel::String cwd() override
    {
        return cwd_ ? cwd_() : keel::String{};
    }

private:
    keel::String name_;
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
