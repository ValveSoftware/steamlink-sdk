// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/EventSourceParser.h"

#include "core/page/EventSource.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/CharacterNames.h"

#include <string.h>

namespace blink {

namespace {

struct EventOrReconnectionTimeSetting {
    enum Type {
        Event,
        ReconnectionTimeSetting,
    };

    EventOrReconnectionTimeSetting(const AtomicString& event, const String& data, const AtomicString& id)
        : type(Event)
        , event(event)
        , data(data)
        , id(id)
        , reconnectionTime(0)
    {
    }
    explicit EventOrReconnectionTimeSetting(unsigned long long reconnectionTime)
        : type(ReconnectionTimeSetting)
        , reconnectionTime(reconnectionTime)
    {
    }

    const Type type;
    const AtomicString event;
    const String data;
    const AtomicString id;
    const unsigned long long reconnectionTime;
};

class Client : public GarbageCollectedFinalized<Client>, public EventSourceParser::Client {
    USING_GARBAGE_COLLECTED_MIXIN(Client);
public:
    ~Client() override {}
    const Vector<EventOrReconnectionTimeSetting>& events() const { return m_events; }
    void onMessageEvent(const AtomicString& event, const String& data, const AtomicString& id) override
    {
        m_events.append(EventOrReconnectionTimeSetting(event, data, id));
    }
    void onReconnectionTimeSet(unsigned long long reconnectionTime) override
    {
        m_events.append(EventOrReconnectionTimeSetting(reconnectionTime));
    }

private:
    Vector<EventOrReconnectionTimeSetting> m_events;
};

class StoppingClient : public GarbageCollectedFinalized<StoppingClient>, public EventSourceParser::Client {
    USING_GARBAGE_COLLECTED_MIXIN(StoppingClient);
public:
    ~StoppingClient() override {}
    const Vector<EventOrReconnectionTimeSetting>& events() const { return m_events; }
    void setParser(EventSourceParser* parser) { m_parser = parser; }
    void onMessageEvent(const AtomicString& event, const String& data, const AtomicString& id) override
    {
        m_parser->stop();
        m_events.append(EventOrReconnectionTimeSetting(event, data, id));
    }
    void onReconnectionTimeSet(unsigned long long reconnectionTime) override
    {
        m_events.append(EventOrReconnectionTimeSetting(reconnectionTime));
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_parser);
        EventSourceParser::Client::trace(visitor);
    }

private:
    Member<EventSourceParser> m_parser;
    Vector<EventOrReconnectionTimeSetting> m_events;
};

class EventSourceParserTest : public ::testing::Test {
protected:
    using Type = EventOrReconnectionTimeSetting::Type;
    EventSourceParserTest()
        : m_client(new Client)
        , m_parser(new EventSourceParser(AtomicString(), m_client))
    {
    }
    ~EventSourceParserTest() override {}

    void enqueue(const char* data) { m_parser->addBytes(data, strlen(data)); }
    void enqueueOneByOne(const char* data)
    {
        const char*p = data;
        while (*p != '\0')
            m_parser->addBytes(p++, 1);
    }

    const Vector<EventOrReconnectionTimeSetting>& events() { return m_client->events(); }

    EventSourceParser* parser() { return m_parser; }

    Persistent<Client> m_client;
    Persistent<EventSourceParser> m_parser;
};

TEST_F(EventSourceParserTest, EmptyMessageEventShouldNotBeDispatched)
{
    enqueue("\n");

    EXPECT_EQ(0u, events().size());
    EXPECT_EQ(String(), parser()->lastEventId());
}

TEST_F(EventSourceParserTest, DispatchSimpleMessageEvent)
{
    enqueue("data:hello\n\n");

    ASSERT_EQ(1u, events().size());
    ASSERT_EQ(Type::Event, events()[0].type);
    EXPECT_EQ("message", events()[0].event);
    EXPECT_EQ("hello", events()[0].data);
    EXPECT_EQ(String(), events()[0].id);
    EXPECT_EQ(AtomicString(), parser()->lastEventId());
}

TEST_F(EventSourceParserTest, ConstructWithLastEventId)
{
    m_parser = new EventSourceParser("hoge", m_client);
    EXPECT_EQ("hoge", parser()->lastEventId());

    enqueue("data:hello\n\n");

    ASSERT_EQ(1u, events().size());
    ASSERT_EQ(Type::Event, events()[0].type);
    EXPECT_EQ("message", events()[0].event);
    EXPECT_EQ("hello", events()[0].data);
    EXPECT_EQ("hoge", events()[0].id);
    EXPECT_EQ("hoge", parser()->lastEventId());
}

TEST_F(EventSourceParserTest, DispatchMessageEventWithLastEventId)
{
    enqueue("id:99\ndata:hello\n");
    EXPECT_EQ(String(), parser()->lastEventId());

    enqueue("\n");

    ASSERT_EQ(1u, events().size());
    ASSERT_EQ(Type::Event, events()[0].type);
    EXPECT_EQ("message", events()[0].event);
    EXPECT_EQ("hello", events()[0].data);
    EXPECT_EQ("99", events()[0].id);
    EXPECT_EQ("99", parser()->lastEventId());
}

TEST_F(EventSourceParserTest, LastEventIdCanBeUpdatedEvenWhenDataIsEmpty)
{
    enqueue("id:99\n");
    EXPECT_EQ(String(), parser()->lastEventId());

    enqueue("\n");

    ASSERT_EQ(0u, events().size());
    EXPECT_EQ("99", parser()->lastEventId());
}

TEST_F(EventSourceParserTest, DispatchMessageEventWithCustomEventType)
{
    enqueue("event:foo\ndata:hello\n\n");

    ASSERT_EQ(1u, events().size());
    ASSERT_EQ(Type::Event, events()[0].type);
    EXPECT_EQ("foo", events()[0].event);
    EXPECT_EQ("hello", events()[0].data);
}

TEST_F(EventSourceParserTest, RetryTakesEffectEvenWhenNotDispatching)
{
    enqueue("retry:999\n");
    ASSERT_EQ(1u, events().size());
    ASSERT_EQ(Type::ReconnectionTimeSetting, events()[0].type);
    ASSERT_EQ(999u, events()[0].reconnectionTime);
}

TEST_F(EventSourceParserTest, EventTypeShouldBeReset)
{
    enqueue("event:foo\ndata:hello\n\ndata:bye\n\n");

    ASSERT_EQ(2u, events().size());
    ASSERT_EQ(Type::Event, events()[0].type);
    EXPECT_EQ("foo", events()[0].event);
    EXPECT_EQ("hello", events()[0].data);

    ASSERT_EQ(Type::Event, events()[1].type);
    EXPECT_EQ("message", events()[1].event);
    EXPECT_EQ("bye", events()[1].data);
}

TEST_F(EventSourceParserTest, DataShouldBeReset)
{
    enqueue("data:hello\n\n\n");

    ASSERT_EQ(1u, events().size());
    ASSERT_EQ(Type::Event, events()[0].type);
    EXPECT_EQ("message", events()[0].event);
    EXPECT_EQ("hello", events()[0].data);
}

TEST_F(EventSourceParserTest, LastEventIdShouldNotBeReset)
{
    enqueue("id:99\ndata:hello\n\ndata:bye\n\n");

    EXPECT_EQ("99", parser()->lastEventId());
    ASSERT_EQ(2u, events().size());
    ASSERT_EQ(Type::Event, events()[0].type);
    EXPECT_EQ("message", events()[0].event);
    EXPECT_EQ("hello", events()[0].data);
    EXPECT_EQ("99", events()[0].id);

    ASSERT_EQ(Type::Event, events()[1].type);
    EXPECT_EQ("message", events()[1].event);
    EXPECT_EQ("bye", events()[1].data);
    EXPECT_EQ("99", events()[1].id);
}

TEST_F(EventSourceParserTest, VariousNewLinesShouldBeAllowed)
{
    enqueueOneByOne("data:hello\r\n\rdata:bye\r\r");

    ASSERT_EQ(2u, events().size());
    ASSERT_EQ(Type::Event, events()[0].type);
    EXPECT_EQ("message", events()[0].event);
    EXPECT_EQ("hello", events()[0].data);

    ASSERT_EQ(Type::Event, events()[1].type);
    EXPECT_EQ("message", events()[1].event);
    EXPECT_EQ("bye", events()[1].data);
}

TEST_F(EventSourceParserTest, RetryWithEmptyValueShouldRestoreDefaultValue)
{
    // TODO(yhirano): This is unspecified in the spec. We need to update
    // the implementation or the spec. See https://crbug.com/587980.
    enqueue("retry\n");
    ASSERT_EQ(1u, events().size());
    ASSERT_EQ(Type::ReconnectionTimeSetting, events()[0].type);
    EXPECT_EQ(EventSource::defaultReconnectDelay, events()[0].reconnectionTime);
}

TEST_F(EventSourceParserTest, NonDigitRetryShouldBeIgnored)
{
    enqueue("retry:a0\n");
    enqueue("retry:xi\n");
    enqueue("retry:2a\n");
    enqueue("retry:09a\n");
    enqueue("retry:1\b\n");
    enqueue("retry:  1234\n");
    enqueue("retry:456 \n");

    EXPECT_EQ(0u, events().size());
}

TEST_F(EventSourceParserTest, UnrecognizedFieldShouldBeIgnored)
{
    enqueue("data:hello\nhoge:fuga\npiyo\n\n");

    ASSERT_EQ(1u, events().size());
    ASSERT_EQ(Type::Event, events()[0].type);
    EXPECT_EQ("message", events()[0].event);
    EXPECT_EQ("hello", events()[0].data);
}

TEST_F(EventSourceParserTest, CommentShouldBeIgnored)
{
    enqueue("data:hello\n:event:a\n\n");

    ASSERT_EQ(1u, events().size());
    ASSERT_EQ(Type::Event, events()[0].type);
    EXPECT_EQ("message", events()[0].event);
    EXPECT_EQ("hello", events()[0].data);
}

TEST_F(EventSourceParserTest, BOMShouldBeIgnored)
{
    // This line is recognized because "\xef\xbb\xbf" is a BOM.
    enqueue("\xef\xbb\xbf" "data:hello\n");
    // This line is ignored because "\xef\xbb\xbf" is part of the field name.
    enqueue("\xef\xbb\xbf" "data:bye\n");
    enqueue("\n");

    ASSERT_EQ(1u, events().size());
    ASSERT_EQ(Type::Event, events()[0].type);
    EXPECT_EQ("message", events()[0].event);
    EXPECT_EQ("hello", events()[0].data);
}

TEST_F(EventSourceParserTest, BOMShouldBeIgnored_OneByOne)
{
    // This line is recognized because "\xef\xbb\xbf" is a BOM.
    enqueueOneByOne("\xef\xbb\xbf" "data:hello\n");
    // This line is ignored because "\xef\xbb\xbf" is part of the field name.
    enqueueOneByOne("\xef\xbb\xbf" "data:bye\n");
    enqueueOneByOne("\n");

    ASSERT_EQ(1u, events().size());
    ASSERT_EQ(Type::Event, events()[0].type);
    EXPECT_EQ("message", events()[0].event);
    EXPECT_EQ("hello", events()[0].data);
}

TEST_F(EventSourceParserTest, ColonlessLineShouldBeTreatedAsNameOnlyField)
{
    enqueue("data:hello\nevent:a\nevent\n\n");

    ASSERT_EQ(1u, events().size());
    ASSERT_EQ(Type::Event, events()[0].type);
    EXPECT_EQ("message", events()[0].event);
    EXPECT_EQ("hello", events()[0].data);
}

TEST_F(EventSourceParserTest, AtMostOneLeadingSpaceCanBeSkipped)
{
    enqueue("data:  hello  \nevent:  type \n\n");

    ASSERT_EQ(1u, events().size());
    ASSERT_EQ(Type::Event, events()[0].type);
    EXPECT_EQ(" type ", events()[0].event);
    EXPECT_EQ(" hello  ", events()[0].data);
}

TEST_F(EventSourceParserTest, DataShouldAccumulate)
{
    enqueue("data\ndata:hello\ndata: world\ndata\n\n");

    ASSERT_EQ(1u, events().size());
    ASSERT_EQ(Type::Event, events()[0].type);
    EXPECT_EQ("message", events()[0].event);
    EXPECT_EQ("\nhello\nworld\n", events()[0].data);
}

TEST_F(EventSourceParserTest, EventShouldNotAccumulate)
{
    enqueue("data:hello\nevent:a\nevent:b\n\n");

    ASSERT_EQ(1u, events().size());
    ASSERT_EQ(Type::Event, events()[0].type);
    EXPECT_EQ("b", events()[0].event);
    EXPECT_EQ("hello", events()[0].data);
}

TEST_F(EventSourceParserTest, FeedDataOneByOne)
{
    enqueueOneByOne("data:hello\r\ndata:world\revent:a\revent:b\nid:4\n\nid:8\ndata:bye\r\n\r");

    ASSERT_EQ(2u, events().size());
    ASSERT_EQ(Type::Event, events()[0].type);
    EXPECT_EQ("b", events()[0].event);
    EXPECT_EQ("hello\nworld", events()[0].data);
    EXPECT_EQ("4", events()[0].id);

    ASSERT_EQ(Type::Event, events()[1].type);
    EXPECT_EQ("message", events()[1].event);
    EXPECT_EQ("bye", events()[1].data);
    EXPECT_EQ("8", events()[1].id);
}

TEST_F(EventSourceParserTest, InvalidUTF8Sequence)
{
    enqueue("data:\xffhello\xc2\ndata:bye\n\n");

    ASSERT_EQ(1u, events().size());
    ASSERT_EQ(Type::Event, events()[0].type);
    EXPECT_EQ("message", events()[0].event);
    String expected = String() + replacementCharacter + "hello" + replacementCharacter + "\nbye";
    EXPECT_EQ(expected, events()[0].data);
}

TEST(EventSourceParserStoppingTest, StopWhileParsing)
{
    StoppingClient* client = new StoppingClient();
    EventSourceParser* parser = new EventSourceParser(AtomicString(), client);
    client->setParser(parser);

    const char input[] = "data:hello\nid:99\n\nid:44\ndata:bye\n\n";
    parser->addBytes(input, strlen(input));

    const auto& events = client->events();

    ASSERT_EQ(1u, events.size());
    ASSERT_EQ(EventOrReconnectionTimeSetting::Type::Event, events[0].type);
    EXPECT_EQ("message", events[0].event);
    EXPECT_EQ("hello", events[0].data);
    EXPECT_EQ("99", parser->lastEventId());
}

} // namespace

} // namespace blink
