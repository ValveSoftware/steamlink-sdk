/*
 * Copyright (C) 2009, 2012 Ericsson AB. All rights reserved.
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef EventSource_h
#define EventSource_h

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/events/EventTarget.h"
#include "core/loader/ThreadableLoader.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "core/page/EventSourceParser.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "wtf/Forward.h"
#include <memory>

namespace blink {

class EventSourceInit;
class ExceptionState;
class ResourceResponse;

class CORE_EXPORT EventSource final : public EventTargetWithInlineData, private ThreadableLoaderClient, public ActiveScriptWrappable, public ActiveDOMObject, public EventSourceParser::Client {
    DEFINE_WRAPPERTYPEINFO();
    USING_GARBAGE_COLLECTED_MIXIN(EventSource);
public:
    static EventSource* create(ExecutionContext*, const String& url, const EventSourceInit&, ExceptionState&);
    ~EventSource() override;

    static const unsigned long long defaultReconnectDelay;

    String url() const;
    bool withCredentials() const;

    enum State : short {
        CONNECTING = 0,
        OPEN = 1,
        CLOSED = 2
    };

    State readyState() const;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(open);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(message);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(error);

    void close();

    const AtomicString& interfaceName() const override;
    ExecutionContext* getExecutionContext() const override;

    // ActiveDOMObject
    //
    // Note: suspend() is noop since ScopedPageLoadDeferrer calls
    // Page::setDefersLoading() and it defers delivery of events from the
    // loader, and therefore the methods of this class for receiving
    // asynchronous events from the loader won't be invoked.
    void stop() override;

    // ActiveScriptWrappable
    bool hasPendingActivity() const final;

    DECLARE_VIRTUAL_TRACE();

private:
    EventSource(ExecutionContext*, const KURL&, const EventSourceInit&);

    void didReceiveResponse(unsigned long, const ResourceResponse&, std::unique_ptr<WebDataConsumerHandle>) override;
    void didReceiveData(const char*, unsigned) override;
    void didFinishLoading(unsigned long, double) override;
    void didFail(const ResourceError&) override;
    void didFailAccessControlCheck(const ResourceError&) override;
    void didFailRedirectCheck() override;

    void onMessageEvent(const AtomicString& event, const String& data, const AtomicString& id) override;
    void onReconnectionTimeSet(unsigned long long reconnectionTime) override;

    void scheduleInitialConnect();
    void connect();
    void networkRequestEnded();
    void scheduleReconnect();
    void connectTimerFired(Timer<EventSource>*);
    void abortConnectionAttempt();

    // The original URL specified when constructing EventSource instance. Used
    // for the 'url' attribute getter.
    const KURL m_url;
    // The URL used to connect to the server, which may be different from
    // |m_url| as it may be redirected.
    KURL m_currentURL;
    bool m_withCredentials;
    State m_state;

    Member<EventSourceParser> m_parser;
    std::unique_ptr<ThreadableLoader> m_loader;
    Timer<EventSource> m_connectTimer;

    unsigned long long m_reconnectDelay;
    String m_eventStreamOrigin;
};

} // namespace blink

#endif // EventSource_h
