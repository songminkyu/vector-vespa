// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include <vespa/messagebus/testlib/receptor.h>
#include <vespa/messagebus/testlib/simplemessage.h>
#include <vespa/messagebus/testlib/simplereply.h>
#include <vespa/messagebus/message.h>
#include <vespa/messagebus/reply.h>
#include <vespa/messagebus/errorcode.h>
#include <vespa/messagebus/error.h>

#include <vespa/vespalib/gtest/gtest.h>

using namespace mbus;
using namespace std::chrono_literals;

TEST(RoutableTest, routable_test) {

    {
        // Test message swap state.
        SimpleMessage foo("foo");
        Route fooRoute = Route::parse("foo");
        foo.setRoute(fooRoute);
        foo.setRetry(1);
        foo.setTimeReceivedNow();
        foo.setTimeRemaining(2ms);

        SimpleMessage bar("bar");
        Route barRoute = Route::parse("bar");
        bar.setRoute(barRoute);
        bar.setRetry(3);
        bar.setTimeReceivedNow();
        bar.setTimeRemaining(4ms);

        foo.swapState(bar);
        EXPECT_EQ(barRoute.toString(), foo.getRoute().toString());
        EXPECT_EQ(fooRoute.toString(), bar.getRoute().toString());
        EXPECT_EQ(3u, foo.getRetry());
        EXPECT_EQ(1u, bar.getRetry());
        EXPECT_TRUE(foo.getTimeReceived() >= bar.getTimeReceived());
        EXPECT_EQ(4ms, foo.getTimeRemaining());
        EXPECT_EQ(2ms, bar.getTimeRemaining());
    }
    {
        // Test reply swap state.
        SimpleReply foo("foo");
        foo.setMessage(Message::UP(new SimpleMessage("foo")));
        foo.setRetryDelay(1);
        foo.addError(Error(ErrorCode::APP_FATAL_ERROR, "fatal"));
        foo.addError(Error(ErrorCode::APP_TRANSIENT_ERROR, "transient"));

        SimpleReply bar("bar");
        bar.setMessage(Message::UP(new SimpleMessage("bar")));
        bar.setRetryDelay(2);
        bar.addError(Error(ErrorCode::ERROR_LIMIT, "err"));

        foo.swapState(bar);
        EXPECT_EQ("bar", static_cast<SimpleMessage&>(*foo.getMessage()).getValue());
        EXPECT_EQ("foo", static_cast<SimpleMessage&>(*bar.getMessage()).getValue());
        EXPECT_EQ(2.0, foo.getRetryDelay());
        EXPECT_EQ(1.0, bar.getRetryDelay());
        EXPECT_EQ(1u, foo.getNumErrors());
        EXPECT_EQ(2u, bar.getNumErrors());
    }
    {
        // Test message discard logic.
        Receptor handler;
        SimpleMessage msg("foo");
        msg.pushHandler(handler);
        msg.discard();

        Reply::UP reply = handler.getReplyNow();
        ASSERT_FALSE(reply);
    }
    {
        // Test reply discard logic.
        Receptor handler;
        SimpleMessage msg("foo");
        msg.pushHandler(handler);

        SimpleReply reply("bar");
        reply.swapState(msg);
        reply.discard();

        Reply::UP ap = handler.getReplyNow();
        ASSERT_FALSE(ap);
    }
}

GTEST_MAIN_RUN_ALL_TESTS()
