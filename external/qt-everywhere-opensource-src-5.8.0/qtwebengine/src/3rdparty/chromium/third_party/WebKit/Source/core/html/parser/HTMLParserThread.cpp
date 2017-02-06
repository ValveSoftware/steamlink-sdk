/*
 * Copyright (C) 2013 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#include "core/html/parser/HTMLParserThread.h"

#include "platform/CrossThreadFunctional.h"
#include "platform/WaitableEvent.h"
#include "platform/heap/SafePoint.h"
#include "public/platform/Platform.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {

static HTMLParserThread* s_sharedThread = nullptr;

HTMLParserThread::HTMLParserThread()
{
}

HTMLParserThread::~HTMLParserThread()
{
}

void HTMLParserThread::init()
{
    ASSERT(!s_sharedThread);
    s_sharedThread = new HTMLParserThread;
}

void HTMLParserThread::setupHTMLParserThread()
{
    ASSERT(m_thread);
    m_thread->initialize();
}

void HTMLParserThread::shutdown()
{
    ASSERT(isMainThread());
    ASSERT(s_sharedThread);
    // currentThread will always be non-null in production, but can be null in Chromium unit tests.
    if (Platform::current()->currentThread() && s_sharedThread->m_thread) {
        WaitableEvent waitableEvent;
        s_sharedThread->postTask(crossThreadBind(&HTMLParserThread::cleanupHTMLParserThread, crossThreadUnretained(s_sharedThread), crossThreadUnretained(&waitableEvent)));
        SafePointScope scope(BlinkGC::HeapPointersOnStack);
        waitableEvent.wait();
    }
    delete s_sharedThread;
    s_sharedThread = nullptr;
}

void HTMLParserThread::cleanupHTMLParserThread(WaitableEvent* waitableEvent)
{
    m_thread->shutdown();
    waitableEvent->signal();
}

HTMLParserThread* HTMLParserThread::shared()
{
    return s_sharedThread;
}

void HTMLParserThread::postTask(std::unique_ptr<CrossThreadClosure> closure)
{
    ASSERT(isMainThread());
    if (!m_thread) {
        m_thread = WebThreadSupportingGC::create("HTMLParserThread");
        postTask(crossThreadBind(&HTMLParserThread::setupHTMLParserThread, crossThreadUnretained(this)));
    }

    m_thread->postTask(BLINK_FROM_HERE, std::move(closure));
}

} // namespace blink
