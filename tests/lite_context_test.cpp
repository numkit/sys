// tests/lite_context_test.cpp
#include <numkit/sys/lite_context.hpp>
#include <gtest/gtest.h>

TEST(LitePluginContextTest, RegisterResolveAndInvoke)
{
    numkit::sys::LitePluginContext ctx;
    EXPECT_NE(ctx.allocator(), nullptr);

    ctx.registerFunction("demo", "add_two", [](keel::ArrayRef<numkit::Value> args,
                                               keel::usize /*nargout*/,
                                               keel::MutArrayRef<numkit::Value> outs,
                                               numkit::CallContext &c) {
        keel::f64 a = args.size() > 0 ? args[0].toScalar() : 0.0;
        keel::f64 b = args.size() > 1 ? args[1].toScalar() : 0.0;
        if (!outs.empty()) outs[0] = numkit::Value::scalar(a + b, c.engine->allocator());
    });

    EXPECT_TRUE(ctx.hasFunction("demo", "add_two"));
    const numkit::ExternalFunc *fn = ctx.resolve("demo", "add_two");
    ASSERT_NE(fn, nullptr);

    numkit::Value in[2] = {numkit::Value::scalar(12.5), numkit::Value::scalar(7.5)};
    numkit::Value out;
    ctx.invokeResolved(fn, keel::ArrayRef<numkit::Value>(in, 2), 1, keel::MutArrayRef<numkit::Value>(&out, 1));
    EXPECT_DOUBLE_EQ(out.toScalar(), 20.0);
}
