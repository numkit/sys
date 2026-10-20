// src/vfs.cpp
//
// Native filesystem implementation for VirtualFS.

#include <numkit/sys/vfs.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace numkit::sys {

namespace {

std::filesystem::path toFsPath(const std::string &utf8)
{
    std::error_code ec;
    try {
        auto p = std::filesystem::u8path(utf8);
        if (std::filesystem::exists(p, ec))
            return p;
        std::filesystem::path p2(utf8);
        if (std::filesystem::exists(p2, ec))
            return p2;
        return p;
    } catch (...) {
        return std::filesystem::path(utf8);
    }
}

std::string toUtf8(const std::filesystem::path &p)
{
    try {
        auto u8 = p.u8string();
        return std::string(reinterpret_cast<const char *>(u8.data()), u8.size());
    } catch (...) {
        return p.string();
    }
}

} // namespace

std::string NativeFS::readFile(const std::string &path)
{
    std::ifstream f(toFsPath(path), std::ios::binary);
    if (!f)
        throw std::runtime_error("cannot open '" + path + "' for reading");
    std::ostringstream os;
    os << f.rdbuf();
    return os.str();
}

void NativeFS::writeFile(const std::string &path, const std::string &content)
{
    std::ofstream f(toFsPath(path), std::ios::binary);
    if (!f)
        throw std::runtime_error("cannot open '" + path + "' for writing");
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!f)
        throw std::runtime_error("write to '" + path + "' failed");
}

bool NativeFS::exists(const std::string &path)
{
    std::error_code ec;
    return std::filesystem::exists(toFsPath(path), ec) && !ec;
}

std::vector<DirEntry> NativeFS::listDir(const std::string &path)
{
    std::vector<DirEntry> out;
    std::error_code ec;
    std::filesystem::directory_iterator it(toFsPath(path), ec);
    if (ec)
        throw std::runtime_error("cannot list directory '" + path + "': " + ec.message());
    for (const auto &entry : it) {
        DirEntry e;
        e.name = toUtf8(entry.path().filename());
        std::error_code dec;
        e.isDirectory = entry.is_directory(dec);
        out.push_back(std::move(e));
    }
    return out;
}

std::optional<FileStat> NativeFS::stat(const std::string &path)
{
    std::error_code ec;
    auto fp = toFsPath(path);
    auto status = std::filesystem::status(fp, ec);
    if (ec || !std::filesystem::exists(status))
        return std::nullopt;

    FileStat fs{};
    if (std::filesystem::is_directory(status)) {
        fs.kind = FileStat::Kind::Directory;
        fs.size = 0;
    } else if (std::filesystem::is_regular_file(status)) {
        fs.kind = FileStat::Kind::File;
        std::error_code sec;
        auto sz = std::filesystem::file_size(fp, sec);
        fs.size = sec ? 0 : static_cast<int64_t>(sz);
    } else {
        fs.kind = FileStat::Kind::Other;
    }

    std::error_code mec;
    auto t = std::filesystem::last_write_time(fp, mec);
    if (!mec) {
        auto dur = t.time_since_epoch();
        fs.mtime = std::chrono::duration_cast<std::chrono::seconds>(dur).count();
    }
    return fs;
}

void NativeFS::mkdir(const std::string &path)
{
    std::error_code ec;
    std::filesystem::create_directories(toFsPath(path), ec);
    if (ec)
        throw std::runtime_error("cannot mkdir '" + path + "': " + ec.message());
}

void NativeFS::rmdir(const std::string &path)
{
    std::error_code ec;
    if (!std::filesystem::remove(toFsPath(path), ec) || ec)
        throw std::runtime_error("cannot rmdir '" + path + "': "
                                  + (ec ? ec.message() : "not empty or absent"));
}

void NativeFS::unlink(const std::string &path)
{
    std::error_code ec;
    if (!std::filesystem::remove(toFsPath(path), ec) || ec)
        throw std::runtime_error("cannot delete '" + path + "': "
                                  + (ec ? ec.message() : "absent"));
}

std::string NativeFS::tempArea()
{
    std::error_code ec;
    auto p = std::filesystem::temp_directory_path(ec);
    if (ec) return {};
    return toUtf8(p);
}

std::string NativeFS::cwd()
{
    std::error_code ec;
    auto p = std::filesystem::current_path(ec);
    if (ec) return {};
    return toUtf8(p);
}

} // namespace numkit::sys
