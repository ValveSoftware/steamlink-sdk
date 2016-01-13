/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "config.h"
#include "public/web/WebKit.h"

#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8GCController.h"
#include "bindings/v8/V8Initializer.h"
#include "core/Init.h"
#include "core/animation/AnimationClock.h"
#include "core/dom/Microtask.h"
#include "core/frame/Settings.h"
#include "core/page/Page.h"
#include "core/workers/WorkerGlobalScopeProxy.h"
#include "gin/public/v8_platform.h"
#include "modules/InitModules.h"
#include "platform/LayoutTestSupport.h"
#include "platform/Logging.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/ImageDecodingStore.h"
#include "platform/graphics/media/MediaPlayer.h"
#include "platform/heap/Heap.h"
#include "platform/heap/glue/MessageLoopInterruptor.h"
#include "platform/heap/glue/PendingGCRunner.h"
#include "public/platform/Platform.h"
#include "public/platform/WebPrerenderingSupport.h"
#include "public/platform/WebThread.h"
#include "web/IndexedDBClientImpl.h"
#include "web/WebMediaPlayerClientImpl.h"
#include "wtf/Assertions.h"
#include "wtf/CryptographicallyRandomNumber.h"
#include "wtf/MainThread.h"
#include "wtf/WTF.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/TextEncoding.h"
#include <v8.h>

namespace blink {

namespace {

class EndOfTaskRunner : public WebThread::TaskObserver {
public:
    virtual void willProcessTask() OVERRIDE
    {
        WebCore::AnimationClock::notifyTaskStart();
    }
    virtual void didProcessTask() OVERRIDE
    {
        WebCore::Microtask::performCheckpoint();
        WebCore::V8GCController::reportDOMMemoryUsageToV8(mainThreadIsolate());
    }
};

} // namespace

static WebThread::TaskObserver* s_endOfTaskRunner = 0;
static WebThread::TaskObserver* s_pendingGCRunner = 0;
static WebCore::ThreadState::Interruptor* s_messageLoopInterruptor = 0;
static WebCore::ThreadState::Interruptor* s_isolateInterruptor = 0;

// Make sure we are not re-initialized in the same address space.
// Doing so may cause hard to reproduce crashes.
static bool s_webKitInitialized = false;

static bool generateEntropy(unsigned char* buffer, size_t length)
{
    if (Platform::current()) {
        Platform::current()->cryptographicallyRandomValues(buffer, length);
        return true;
    }
    return false;
}

void initialize(Platform* platform)
{
    initializeWithoutV8(platform);

    v8::V8::InitializePlatform(gin::V8Platform::Get());
    v8::Isolate* isolate = v8::Isolate::New();
    isolate->Enter();
    WebCore::V8Initializer::initializeMainThreadIfNeeded(isolate);
    v8::V8::SetEntropySource(&generateEntropy);
    v8::V8::SetArrayBufferAllocator(WebCore::v8ArrayBufferAllocator());
    v8::V8::Initialize();
    WebCore::V8PerIsolateData::ensureInitialized(isolate);

    s_isolateInterruptor = new WebCore::V8IsolateInterruptor(v8::Isolate::GetCurrent());
    WebCore::ThreadState::current()->addInterruptor(s_isolateInterruptor);

    // currentThread will always be non-null in production, but can be null in Chromium unit tests.
    if (WebThread* currentThread = platform->currentThread()) {
        ASSERT(!s_endOfTaskRunner);
        s_endOfTaskRunner = new EndOfTaskRunner;
        currentThread->addTaskObserver(s_endOfTaskRunner);
    }
}

v8::Isolate* mainThreadIsolate()
{
    return WebCore::V8PerIsolateData::mainThreadIsolate();
}

static double currentTimeFunction()
{
    return Platform::current()->currentTime();
}

static double monotonicallyIncreasingTimeFunction()
{
    return Platform::current()->monotonicallyIncreasingTime();
}

static void cryptographicallyRandomValues(unsigned char* buffer, size_t length)
{
    Platform::current()->cryptographicallyRandomValues(buffer, length);
}

static void callOnMainThreadFunction(WTF::MainThreadFunction function, void* context)
{
    Platform::current()->callOnMainThread(function, context);
}

void initializeWithoutV8(Platform* platform)
{
    ASSERT(!s_webKitInitialized);
    s_webKitInitialized = true;

    ASSERT(platform);
    Platform::initialize(platform);

    WTF::setRandomSource(cryptographicallyRandomValues);
    WTF::initialize(currentTimeFunction, monotonicallyIncreasingTimeFunction);
    WTF::initializeMainThread(callOnMainThreadFunction);
    WebCore::Heap::init();

    WebCore::ThreadState::attachMainThread();
    // currentThread will always be non-null in production, but can be null in Chromium unit tests.
    if (WebThread* currentThread = platform->currentThread()) {
        ASSERT(!s_pendingGCRunner);
        s_pendingGCRunner = new WebCore::PendingGCRunner;
        currentThread->addTaskObserver(s_pendingGCRunner);

        ASSERT(!s_messageLoopInterruptor);
        s_messageLoopInterruptor = new WebCore::MessageLoopInterruptor(currentThread);
        WebCore::ThreadState::current()->addInterruptor(s_messageLoopInterruptor);
    }

    DEFINE_STATIC_LOCAL(WebCore::ModulesInitializer, initializer, ());
    initializer.init();

    // There are some code paths (for example, running WebKit in the browser
    // process and calling into LocalStorage before anything else) where the
    // UTF8 string encoding tables are used on a background thread before
    // they're set up.  This is a problem because their set up routines assert
    // they're running on the main WebKitThread.  It might be possible to make
    // the initialization thread-safe, but given that so many code paths use
    // this, initializing this lazily probably doesn't buy us much.
    WTF::UTF8Encoding();

    WebCore::setIndexedDBClientCreateFunction(blink::IndexedDBClientImpl::create);

    WebCore::MediaPlayer::setMediaEngineCreateFunction(blink::WebMediaPlayerClientImpl::create);
}

void shutdown()
{
    // currentThread will always be non-null in production, but can be null in Chromium unit tests.
    if (Platform::current()->currentThread()) {
        ASSERT(s_endOfTaskRunner);
        Platform::current()->currentThread()->removeTaskObserver(s_endOfTaskRunner);
        delete s_endOfTaskRunner;
        s_endOfTaskRunner = 0;
    }

    ASSERT(s_isolateInterruptor);
    WebCore::ThreadState::current()->removeInterruptor(s_isolateInterruptor);

    // currentThread will always be non-null in production, but can be null in Chromium unit tests.
    if (Platform::current()->currentThread()) {
        ASSERT(s_pendingGCRunner);
        delete s_pendingGCRunner;
        s_pendingGCRunner = 0;

        ASSERT(s_messageLoopInterruptor);
        WebCore::ThreadState::current()->removeInterruptor(s_messageLoopInterruptor);
        delete s_messageLoopInterruptor;
        s_messageLoopInterruptor = 0;
    }

    // Detach the main thread before starting the shutdown sequence
    // so that the main thread won't get involved in a GC during the shutdown.
    WebCore::ThreadState::detachMainThread();

    v8::Isolate* isolate = WebCore::V8PerIsolateData::mainThreadIsolate();
    WebCore::V8PerIsolateData::dispose(isolate);
    isolate->Exit();
    isolate->Dispose();

    shutdownWithoutV8();
}

void shutdownWithoutV8()
{
    ASSERT(!s_endOfTaskRunner);
    WebCore::shutdown();
    WebCore::Heap::shutdown();
    WTF::shutdown();
    Platform::shutdown();
    WebPrerenderingSupport::shutdown();
}

void setLayoutTestMode(bool value)
{
    WebCore::setIsRunningLayoutTest(value);
}

bool layoutTestMode()
{
    return WebCore::isRunningLayoutTest();
}

void setFontAntialiasingEnabledForTest(bool value)
{
    WebCore::setFontAntialiasingEnabledForTest(value);
}

bool fontAntialiasingEnabledForTest()
{
    return WebCore::isFontAntialiasingEnabledForTest();
}

void enableLogChannel(const char* name)
{
#if !LOG_DISABLED
    WTFLogChannel* channel = WebCore::getChannelFromName(name);
    if (channel)
        channel->state = WTFLogChannelOn;
#endif // !LOG_DISABLED
}

void resetPluginCache(bool reloadPages)
{
    WebCore::Page::refreshPlugins(reloadPages);
}

} // namespace blink
