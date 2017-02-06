// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/peerconnection/RTCDataChannel.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMException.h"
#include "core/events/Event.h"
#include "platform/heap/Heap.h"
#include "public/platform/WebRTCDataChannelHandler.h"
#include "public/platform/WebVector.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {
namespace {

class MockHandler final : public WebRTCDataChannelHandler {
public:
    MockHandler()
        : m_client(0)
        , m_state(WebRTCDataChannelHandlerClient::ReadyStateConnecting)
        , m_bufferedAmount(0)
    {
    }
    void setClient(WebRTCDataChannelHandlerClient* client) override { m_client = client; }
    WebString label() override { return WebString(""); }
    bool ordered() const override { return true; }
    unsigned short maxRetransmitTime() const override { return 0; }
    unsigned short maxRetransmits() const override { return 0; }
    WebString protocol() const override { return WebString(""); }
    bool negotiated() const override { return false; }
    unsigned short id() const override { return 0; }

    WebRTCDataChannelHandlerClient::ReadyState state() const override { return m_state; }
    unsigned long bufferedAmount() override { return m_bufferedAmount; }
    bool sendStringData(const WebString& s) override
    {
        m_bufferedAmount += s.length();
        return true;
    }
    bool sendRawData(const char* data, size_t length) override
    {
        m_bufferedAmount += length;
        return true;
    }
    void close() override { }

    // Methods for testing.
    void changeState(WebRTCDataChannelHandlerClient::ReadyState state)
    {
        m_state = state;
        if (m_client) {
            m_client->didChangeReadyState(m_state);
        }
    }
    void drainBuffer(unsigned long bytes)
    {
        unsigned long oldBufferedAmount = m_bufferedAmount;
        m_bufferedAmount -= bytes;
        if (m_client) {
            m_client->didDecreaseBufferedAmount(oldBufferedAmount);
        }
    }

private:
    WebRTCDataChannelHandlerClient* m_client;
    WebRTCDataChannelHandlerClient::ReadyState m_state;
    unsigned long m_bufferedAmount;
};

} // namespace

TEST(RTCDataChannelTest, BufferedAmount)
{
    MockHandler* handler = new MockHandler();
    RTCDataChannel* channel = RTCDataChannel::create(0, wrapUnique(handler));

    handler->changeState(WebRTCDataChannelHandlerClient::ReadyStateOpen);
    String message(std::string(100, 'A').c_str());
    channel->send(message, IGNORE_EXCEPTION);
    EXPECT_EQ(100U, channel->bufferedAmount());
}

TEST(RTCDataChannelTest, BufferedAmountLow)
{
    MockHandler* handler = new MockHandler();
    RTCDataChannel* channel = RTCDataChannel::create(0, wrapUnique(handler));

    // Add and drain 100 bytes
    handler->changeState(WebRTCDataChannelHandlerClient::ReadyStateOpen);
    String message(std::string(100, 'A').c_str());
    channel->send(message, IGNORE_EXCEPTION);
    EXPECT_EQ(100U, channel->bufferedAmount());
    EXPECT_EQ(1U, channel->m_scheduledEvents.size());

    handler->drainBuffer(100);
    EXPECT_EQ(0U, channel->bufferedAmount());
    EXPECT_EQ(2U, channel->m_scheduledEvents.size());
    EXPECT_EQ("bufferedamountlow", std::string(channel->m_scheduledEvents.last()->type().utf8().data()));

    // Add and drain 1 byte
    channel->send("A", IGNORE_EXCEPTION);
    EXPECT_EQ(1U, channel->bufferedAmount());
    EXPECT_EQ(2U, channel->m_scheduledEvents.size());

    handler->drainBuffer(1);
    EXPECT_EQ(0U, channel->bufferedAmount());
    EXPECT_EQ(3U, channel->m_scheduledEvents.size());
    EXPECT_EQ("bufferedamountlow", std::string(channel->m_scheduledEvents.last()->type().utf8().data()));

    // Set the threshold to 99 bytes, add 101, and drain 1 byte at a time.
    channel->setBufferedAmountLowThreshold(99U);
    channel->send(message, IGNORE_EXCEPTION);
    EXPECT_EQ(100U, channel->bufferedAmount());

    channel->send("A", IGNORE_EXCEPTION);
    EXPECT_EQ(101U, channel->bufferedAmount());

    handler->drainBuffer(1);
    EXPECT_EQ(100U, channel->bufferedAmount());
    EXPECT_EQ(3U, channel->m_scheduledEvents.size()); // No new events.

    handler->drainBuffer(1);
    EXPECT_EQ(99U, channel->bufferedAmount());
    EXPECT_EQ(4U, channel->m_scheduledEvents.size()); // One new event.
    EXPECT_EQ("bufferedamountlow", std::string(channel->m_scheduledEvents.last()->type().utf8().data()));

    handler->drainBuffer(1);
    EXPECT_EQ(98U, channel->bufferedAmount());

    channel->setBufferedAmountLowThreshold(97U);
    EXPECT_EQ(4U, channel->m_scheduledEvents.size()); // No new events.

    handler->drainBuffer(1);
    EXPECT_EQ(97U, channel->bufferedAmount());
    EXPECT_EQ(5U, channel->m_scheduledEvents.size()); // New event.
    EXPECT_EQ("bufferedamountlow", std::string(channel->m_scheduledEvents.last()->type().utf8().data()));
}

} // namespace blink
