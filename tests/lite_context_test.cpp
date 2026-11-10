// tests/lite_context_test.cpp
#include <numkit/sys/lite_context.hpp>
#include <gtest/gtest.h>

TEST(LitePluginContextTest, RegisterResolveAndInvoke)
{
    numkit::sys::LitePluginContext ctx;
    ctx.registerFunction("demo", "add_two", [](numkit::Span<const numkit::Value> args,
                                               size_t /*nargout*/,
                                               numkit::Span<numkit::Value> outs,
                                               numkit::CallContext & /*c*/) {
        double a = args.size() > 0 ? args[0].toScalar() : 0.0;
        double b = args.size() > 1 ? args[1].toScalar() : 0.0;
        if (!outs.empty()) outs[0] = numkit::Value::scalar(a + b);
    });

    EXPECT_TRUE(ctx.hasFunction("demo", "add_two"));
    const numkit::ExternalFunc *fn = ctx.resolve("demo", "add_two");
    ASSERT_NE(fn, nullptr);

    numkit::Value in[2] = {numkit::Value::scalar(12.5), numkit::Value::scalar(7.5)};
    numkit::Value out;
    ctx.invokeResolved(fn, numkit::Span<const numkit::Value>(in, 2), 1, numkit::Span<numkit::Value>(&out, 1));
    EXPECT_DOUBLE_EQ(out.toScalar(), 20.0);
}
