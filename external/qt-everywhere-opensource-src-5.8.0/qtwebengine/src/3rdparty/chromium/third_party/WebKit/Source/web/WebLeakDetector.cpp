/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#include "public/web/WebLeakDetector.h"

#include "bindings/core/v8/V8GCController.h"
#include "core/editing/spellcheck/SpellChecker.h"
#include "core/fetch/MemoryCache.h"
#include "core/inspector/InstanceCounters.h"
#include "core/workers/InProcessWorkerMessagingProxy.h"
#include "core/workers/WorkerThread.h"
#include "platform/Timer.h"
#include "public/web/WebFrame.h"
#include "web/WebLocalFrameImpl.h"

namespace blink {

namespace {

class WebLeakDetectorImpl final : public WebLeakDetector {
WTF_MAKE_NONCOPYABLE(WebLeakDetectorImpl);
public:
    explicit WebLeakDetectorImpl(WebLeakDetectorClient* client)
        : m_client(client)
        , m_delayedGCAndReportTimer(this, &WebLeakDetectorImpl::delayedGCAndReport)
        , m_delayedReportTimer(this, &WebLeakDetectorImpl::delayedReport)
        , m_numberOfGCNeeded(0)
    {
        DCHECK(m_client);
    }

    ~WebLeakDetectorImpl() override {}

    void prepareForLeakDetection(WebFrame*) override;
    void collectGarbageAndReport() override;

private:
    void delayedGCAndReport(Timer<WebLeakDetectorImpl>*);
    void delayedReport(Timer<WebLeakDetectorImpl>*);

    WebLeakDetectorClient* m_client;
    Timer<WebLeakDetectorImpl> m_delayedGCAndReportTimer;
    Timer<WebLeakDetectorImpl> m_delayedReportTimer;
    int m_numberOfGCNeeded;
};

void WebLeakDetectorImpl::prepareForLeakDetection(WebFrame* frame)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope handleScope(isolate);

    // For example, calling isValidEmailAddress in EmailInputType.cpp with a
    // non-empty string creates a static ScriptRegexp value which holds a
    // V8PerContextData indirectly. This affects the number of V8PerContextData.
    // To ensure that context data is created, call ensureScriptRegexpContext
    // here.
    V8PerIsolateData::from(isolate)->ensureScriptRegexpContext();

    WorkerThread::terminateAndWaitForAllWorkers();
    memoryCache()->evictResources();

    // If the spellchecker is allowed to continue issuing requests while the
    // leak detector runs, leaks may flakily be reported as the requests keep
    // their associated element (and document) alive.
    //
    // Stop the spellchecker to prevent this.
    if (frame->isWebLocalFrame()) {
        WebLocalFrameImpl* localFrame = toWebLocalFrameImpl(frame);
        SpellChecker& spellChecker = localFrame->frame()->spellChecker();
        spellChecker.prepareForLeakDetection();
    }

    // FIXME: HTML5 Notification should be closed because notification affects the result of number of DOM objects.

    V8PerIsolateData::from(isolate)->clearScriptRegexpContext();
}

void WebLeakDetectorImpl::collectGarbageAndReport()
{
    V8GCController::collectAllGarbageForTesting(V8PerIsolateData::mainThreadIsolate());
    // Note: Oilpan precise GC is scheduled at the end of the event loop.

    // Task queue may contain delayed object destruction tasks.
    // This method is called from navigation hook inside FrameLoader,
    // so previous document is still held by the loader until the next event loop.
    // Complete all pending tasks before proceeding to gc.
    m_numberOfGCNeeded = 2;
    m_delayedGCAndReportTimer.startOneShot(0, BLINK_FROM_HERE);
}

void WebLeakDetectorImpl::delayedGCAndReport(Timer<WebLeakDetectorImpl>*)
{
    // We do a second and third GC here to address flakiness
    // The second GC is necessary as Resource GC may have postponed clean-up tasks to next event loop.
    // The third GC is necessary for cleaning up Document after worker object died.

    V8GCController::collectAllGarbageForTesting(V8PerIsolateData::mainThreadIsolate());
    // Note: Oilpan precise GC is scheduled at the end of the event loop.

    // Inspect counters on the next event loop.
    if (--m_numberOfGCNeeded > 0) {
        m_delayedGCAndReportTimer.startOneShot(0, BLINK_FROM_HERE);
    } else if (m_numberOfGCNeeded > -1 && InProcessWorkerMessagingProxy::proxyCount()) {
        // It is possible that all posted tasks for finalizing in-process proxy objects
        // will not have run before the final round of GCs started. If so, do yet
        // another pass, letting these tasks run and then afterwards perform a GC to tidy up.
        //
        // TODO(sof): use proxyCount() to always decide if another GC needs to be scheduled.
        // Some debug bots running browser unit tests disagree (crbug.com/616714)
        m_delayedGCAndReportTimer.startOneShot(0, BLINK_FROM_HERE);
    } else {
        m_delayedReportTimer.startOneShot(0, BLINK_FROM_HERE);
    }
}

void WebLeakDetectorImpl::delayedReport(Timer<WebLeakDetectorImpl>*)
{
    DCHECK(m_client);

    WebLeakDetectorClient::Result result;
    result.numberOfLiveAudioNodes = InstanceCounters::counterValue(InstanceCounters::AudioHandlerCounter);
    result.numberOfLiveDocuments = InstanceCounters::counterValue(InstanceCounters::DocumentCounter);
    result.numberOfLiveNodes = InstanceCounters::counterValue(InstanceCounters::NodeCounter);
    result.numberOfLiveLayoutObjects = InstanceCounters::counterValue(InstanceCounters::LayoutObjectCounter);
    result.numberOfLiveResources = InstanceCounters::counterValue(InstanceCounters::ResourceCounter);
    result.numberOfLiveActiveDOMObjects = InstanceCounters::counterValue(InstanceCounters::ActiveDOMObjectCounter);
    result.numberOfLiveScriptPromises = InstanceCounters::counterValue(InstanceCounters::ScriptPromiseCounter);
    result.numberOfLiveFrames = InstanceCounters::counterValue(InstanceCounters::FrameCounter);
    result.numberOfLiveV8PerContextData = InstanceCounters::counterValue(InstanceCounters::V8PerContextDataCounter);

    m_client->onLeakDetectionComplete(result);

#ifndef NDEBUG
    showLiveDocumentInstances();
#endif
}

} // namespace

WebLeakDetector* WebLeakDetector::create(WebLeakDetectorClient* client)
{
    return new WebLeakDetectorImpl(client);
}

} // namespace blink
