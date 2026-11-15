// src/vfs.cpp
//
// Native filesystem implementation for VirtualFS backed by keel::fs_*.

#include <numkit/sys/vfs.hpp>

#include <keel/arena.h>
#include <keel/fs.h>

#if !defined(_WIN32)
#  include <fcntl.h>
#  include <sys/stat.h>
#  include <time.h>
#endif

namespace numkit::sys {

std::string NativeFS::readFile(const std::string &path)
{
    keel::ArenaTemp scratch = keel::get_scratch();
    auto res = keel::fs_read_text(keel::StringRef(path), scratch.allocator());
    if (res.is_err())
        throw std::runtime_error("cannot open '" + path + "' for reading");
    const keel::String &s = res.value();
    return std::string(s.data(), s.size());
}

void NativeFS::writeFile(const std::string &path, const std::string &content)
{
    keel::Error err = keel::fs_write_text(keel::StringRef(path), keel::StringRef(content));
    if (err.is_err())
        throw std::runtime_error("cannot open '" + path + "' for writing");
#if !defined(_WIN32)
    struct timespec ts[2];
    if (::clock_gettime(CLOCK_REALTIME, &ts[0]) == 0) {
        ts[1] = ts[0];
        (void)::utimensat(AT_FDCWD, path.c_str(), ts, 0);
    }
#endif
}

bool NativeFS::exists(const std::string &path)
{
    return keel::fs_exists(keel::StringRef(path));
}

std::vector<DirEntry> NativeFS::listDir(const std::string &path)
{
    std::vector<DirEntry> out;
    keel::Error err = keel::fs_read_dir(keel::StringRef(path), [&](const keel::DirEntry &de) {
        DirEntry e;
        e.name = std::string(de.name.data(), de.name.size());
        e.isDirectory = de.is_dir;
        out.push_back(std::move(e));
        return true;
    });
    if (err.is_err()) {
        const keel::StringRef msg = keel::error_message(err);
        throw std::runtime_error("cannot list directory '" + path + "': "
                                 + std::string(msg.data(), msg.size()));
    }
    return out;
}

std::optional<FileStat> NativeFS::stat(const std::string &path)
{
    auto res = keel::fs_stat(keel::StringRef(path));
    if (res.is_err())
        return std::nullopt;

    const keel::FileStat &st = res.value();
    FileStat out{};
    if (st.is_dir) {
        out.kind = FileStat::Kind::Directory;
        out.size = 0;
    } else if (st.is_file) {
        out.kind = FileStat::Kind::File;
        out.size = static_cast<keel::i64>(st.size);
    } else {
        out.kind = FileStat::Kind::Other;
    }
    out.mtimeNs = st.mtime_ns;
    out.mtime = st.mtime_ns / 1000000000LL;
    return out;
}

void NativeFS::mkdir(const std::string &path)
{
    keel::Error err = keel::fs_create_dirs(keel::StringRef(path));
    if (err.is_err()) {
        const keel::StringRef msg = keel::error_message(err);
        throw std::runtime_error("cannot mkdir '" + path + "': "
                                 + std::string(msg.data(), msg.size()));
    }
}

void NativeFS::rmdir(const std::string &path)
{
    auto st = keel::fs_stat(keel::StringRef(path));
    if (st.is_err() || !st.value().is_dir)
        throw std::runtime_error("cannot rmdir '" + path + "': not a directory or absent");
    keel::Error err = keel::fs_remove(keel::StringRef(path));
    if (err.is_err()) {
        const keel::StringRef msg = keel::error_message(err);
        throw std::runtime_error("cannot rmdir '" + path + "': "
                                 + std::string(msg.data(), msg.size()));
    }
}

void NativeFS::unlink(const std::string &path)
{
    auto st = keel::fs_stat(keel::StringRef(path));
    if (st.is_err() || st.value().is_dir)
        throw std::runtime_error("cannot delete '" + path + "': absent or directory");
    keel::Error err = keel::fs_remove(keel::StringRef(path));
    if (err.is_err()) {
        const keel::StringRef msg = keel::error_message(err);
        throw std::runtime_error("cannot delete '" + path + "': "
                                 + std::string(msg.data(), msg.size()));
    }
}

std::string NativeFS::tempArea()
{
    keel::ArenaTemp scratch = keel::get_scratch();
    auto res = keel::fs_temp_dir(scratch.allocator());
    if (res.is_err()) return {};
    const keel::String &s = res.value();
    return std::string(s.data(), s.size());
}

std::string NativeFS::cwd()
{
    keel::ArenaTemp scratch = keel::get_scratch();
    auto res = keel::fs_current_dir(scratch.allocator());
    if (res.is_err()) return {};
    const keel::String &s = res.value();
    return std::string(s.data(), s.size());
}

} // namespace numkit::sys
