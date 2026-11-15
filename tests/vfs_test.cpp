// tests/vfs_test.cpp
#include <numkit/sys/vfs.hpp>
#include <numkit/sys/fs_context.hpp>
#include <keel/format.h>
#include <gtest/gtest.h>

using namespace numkit::sys;

TEST(VfsTest, NativeFSReadWriteExists)
{
    NativeFS fs;
    keel::String tmpDir = fs.tempArea();
    ASSERT_FALSE(tmpDir.empty());

    keel::String testFile = keel::format("{}/numkit_vfs_test.txt", tmpDir);
    keel::String sample = keel::String::from("NumKit Sys VFS Unit Test Content\n");

    fs.writeFile(testFile, sample);
    EXPECT_TRUE(fs.exists(testFile));

    keel::String readBack = fs.readFile(testFile);
    EXPECT_EQ(readBack, sample);

    auto statInfo = fs.stat(testFile);
    ASSERT_TRUE(statInfo.has_value());
    EXPECT_EQ(statInfo->kind, FileStat::Kind::File);
    EXPECT_EQ(statInfo->size, static_cast<keel::i64>(sample.size()));

    fs.unlink(testFile);
    EXPECT_FALSE(fs.exists(testFile));
}

TEST(VfsTest, FsContextPathResolution)
{
    FsContext ctx;
    ctx.registerVirtualFS(keel::make_unique<NativeFS>());

    auto resolved = ctx.resolvePath("native:/test/path.txt");
    ASSERT_NE(resolved.fs, nullptr);
    EXPECT_EQ(resolved.fs->name(), "native");
}
