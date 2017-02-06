// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPepperSocketChannelClientProxy_h
#define WebPepperSocketChannelClientProxy_h

#include "modules/websockets/WebSocketChannelClient.h"
#include "platform/heap/Handle.h"
#include "web/WebPepperSocketImpl.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"
#include <memory>
#include <stdint.h>

namespace blink {

// Ideally we want to simply make WebPepperSocketImpl inherit from
// WebPepperSocketChannelClient, but we cannot do that because
// WebSocketChannelClient needs to be on Oilpan's heap whereas
// WebPepperSocketImpl cannot be on Oilpan's heap. Thus we need to introduce a
// proxy class to decouple WebPepperSocketImpl from WebSocketChannelClient.
class WebPepperSocketChannelClientProxy final : public GarbageCollectedFinalized<WebPepperSocketChannelClientProxy>, public WebSocketChannelClient {
    USING_GARBAGE_COLLECTED_MIXIN(WebPepperSocketChannelClientProxy)
public:
    static WebPepperSocketChannelClientProxy* create(WebPepperSocketImpl* impl)
    {
        return new WebPepperSocketChannelClientProxy(impl);
    }

    void didConnect(const String& subprotocol, const String& extensions) override
    {
        m_impl->didConnect(subprotocol, extensions);
    }
    void didReceiveTextMessage(const String& payload) override
    {
        m_impl->didReceiveTextMessage(payload);
    }
    void didReceiveBinaryMessage(std::unique_ptr<Vector<char>> payload) override
    {
        m_impl->didReceiveBinaryMessage(std::move(payload));
    }
    void didError() override
    {
        m_impl->didError();
    }
    void didConsumeBufferedAmount(uint64_t consumed) override
    {
        m_impl->didConsumeBufferedAmount(consumed);
    }
    void didStartClosingHandshake() override
    {
        m_impl->didStartClosingHandshake();
    }
    void didClose(ClosingHandshakeCompletionStatus status, unsigned short code, const String& reason) override
    {
        WebPepperSocketImpl* impl = m_impl;
        m_impl = nullptr;
        impl->didClose(status, code, reason);
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        WebSocketChannelClient::trace(visitor);
    }

private:
    explicit WebPepperSocketChannelClientProxy(WebPepperSocketImpl* impl)
        : m_impl(impl)
    {
    }

    WebPepperSocketImpl* m_impl;
};

} // namespace blink

#endif // WebPepperSocketChannelClientProxy_h
