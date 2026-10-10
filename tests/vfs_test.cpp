// tests/vfs_test.cpp
#include <numkit/sys/vfs.hpp>
#include <numkit/sys/fs_context.hpp>
#include <gtest/gtest.h>

using namespace numkit::sys;

TEST(VfsTest, NativeFSReadWriteExists)
{
    NativeFS fs;
    std::string tmpDir = fs.tempArea();
    ASSERT_FALSE(tmpDir.empty());

    std::string testFile = tmpDir + "/numkit_vfs_test.txt";
    std::string sample = "NumKit Sys VFS Unit Test Content\n";

    fs.writeFile(testFile, sample);
    EXPECT_TRUE(fs.exists(testFile));

    std::string readBack = fs.readFile(testFile);
    EXPECT_EQ(readBack, sample);

    auto statInfo = fs.stat(testFile);
    ASSERT_TRUE(statInfo.has_value());
    EXPECT_EQ(statInfo->kind, FileStat::Kind::File);
    EXPECT_EQ(statInfo->size, static_cast<int64_t>(sample.size()));

    fs.unlink(testFile);
    EXPECT_FALSE(fs.exists(testFile));
}

TEST(VfsTest, FsContextPathResolution)
{
    FsContext ctx;
    ctx.registerVirtualFS(std::make_unique<NativeFS>());

    auto resolved = ctx.resolvePath("native:/test/path.txt");
    ASSERT_NE(resolved.fs, nullptr);
    EXPECT_EQ(resolved.fs->name(), "native");
}
