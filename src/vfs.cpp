// src/vfs.cpp
//
// Native filesystem implementation for VirtualFS backed by keel::fs_*.

#include <numkit/sys/vfs.hpp>

#include <keel/format.h>
#include <keel/exception.h>
#include <keel/fs.h>

namespace numkit::sys {

keel::String NativeFS::readFile(keel::StringRef path)
{
    auto res = keel::fs_read_text(path);
    if (res.is_err())
        throw keel::Exception(keel::format("cannot open '{}' for reading", path).c_str());
    return keel::move(res.value());
}

void NativeFS::writeFile(keel::StringRef path, keel::StringRef content)
{
    keel::Error err = keel::fs_write_text(path, content);
    if (err.is_err())
        throw keel::Exception(keel::format("cannot open '{}' for writing", path).c_str());
    (void)keel::fs_set_mtime_now(path);
}

bool NativeFS::exists(keel::StringRef path)
{
    return keel::fs_exists(path);
}

keel::Array<DirEntry> NativeFS::listDir(keel::StringRef path)
{
    keel::Array<DirEntry> out;
    keel::Error err = keel::fs_read_dir(path, [&](const keel::DirEntry &de) {
        DirEntry e;
        e.name = keel::String(de.name);
        e.isDirectory = de.is_dir;
        out.push_back(keel::move(e));
        return true;
    });
    if (err.is_err()) {
        const keel::StringRef msg = keel::error_message(err);
        throw keel::Exception(
            keel::format("cannot list directory '{}': {}", path, msg).c_str());
    }
    return out;
}

keel::Optional<FileStat> NativeFS::stat(keel::StringRef path)
{
    auto res = keel::fs_stat(path);
    if (res.is_err())
        return keel::kNullOpt;

    const keel::FileStat &st = res.value();
    FileStat out{};
    if (st.is_dir) {
        out.kind = FileStat::Kind::Directory;
        out.size = 0;
    } else if (st.is_file) {
        out.kind = FileStat::Kind::File;
        out.size = keel::as<keel::i64>(st.size);
    } else {
        out.kind = FileStat::Kind::Other;
    }
    out.mtimeNs = st.mtime_ns;
    out.mtime = st.mtime_ns / 1000000000LL;
    return out;
}

void NativeFS::mkdir(keel::StringRef path)
{
    keel::Error err = keel::fs_create_dirs(path);
    if (err.is_err()) {
        const keel::StringRef msg = keel::error_message(err);
        throw keel::Exception(keel::format("cannot mkdir '{}': {}", path, msg).c_str());
    }
}

void NativeFS::rmdir(keel::StringRef path)
{
    auto st = keel::fs_stat(path);
    if (st.is_err() || !st.value().is_dir)
        throw keel::Exception(
            keel::format("cannot rmdir '{}': not a directory or absent", path).c_str());
    keel::Error err = keel::fs_remove(path);
    if (err.is_err()) {
        const keel::StringRef msg = keel::error_message(err);
        throw keel::Exception(keel::format("cannot rmdir '{}': {}", path, msg).c_str());
    }
}

void NativeFS::unlink(keel::StringRef path)
{
    auto st = keel::fs_stat(path);
    if (st.is_err() || st.value().is_dir)
        throw keel::Exception(
            keel::format("cannot delete '{}': absent or directory", path).c_str());
    keel::Error err = keel::fs_remove(path);
    if (err.is_err()) {
        const keel::StringRef msg = keel::error_message(err);
        throw keel::Exception(keel::format("cannot delete '{}': {}", path, msg).c_str());
    }
}

keel::String NativeFS::tempArea()
{
    auto res = keel::fs_temp_dir();
    if (res.is_err()) return {};
    return keel::move(res.value());
}

keel::String NativeFS::cwd()
{
    auto res = keel::fs_current_dir();
    if (res.is_err()) return {};
    return keel::move(res.value());
}

} // namespace numkit::sys
