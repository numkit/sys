// tests/process_test.cpp
#include <numkit/sys/process.hpp>
#include <gtest/gtest.h>

using namespace numkit::sys;

TEST(ProcessTest, DetectsStandardCommands)
{
#ifdef _WIN32
    EXPECT_TRUE(isCommandAvailable("cmd"));
    EXPECT_TRUE(isCommandAvailable("where"));
#else
    EXPECT_TRUE(isCommandAvailable("sh"));
    EXPECT_TRUE(isCommandAvailable("ls"));
#endif
    EXPECT_FALSE(isCommandAvailable("numkit_non_existent_cmd_abcdef_9999"));
}

TEST(ProcessTest, RunsCommandAndCapturesOutput)
{
#ifdef _WIN32
    auto res = runProcess("cmd.exe /c echo hello_numkit_process");
#else
    auto res = runProcess("echo hello_numkit_process");
#endif

    EXPECT_TRUE(res.ok());
    EXPECT_EQ(res.exitCode, 0);
    EXPECT_NE(res.output.find("hello_numkit_process"), std::string::npos);
}

TEST(ProcessTest, CapturesFailureExitCode)
{
#ifdef _WIN32
    auto res = runProcess("cmd.exe /c exit 42");
    EXPECT_FALSE(res.ok());
    EXPECT_EQ(res.exitCode, 42);
#else
    auto res = runProcess("sh -c 'exit 42'");
    EXPECT_FALSE(res.ok());
    EXPECT_EQ(res.exitCode, 42);
#endif
}
