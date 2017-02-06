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

#include "public/web/WebKit.h"

#include "bindings/core/v8/Microtask.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8GCController.h"
#include "bindings/core/v8/V8Initializer.h"
#include "core/animation/AnimationClock.h"
#include "core/page/Page.h"
#include "core/workers/WorkerBackingThread.h"
#include "gin/public/v8_platform.h"
#include "modules/ModulesInitializer.h"
#include "platform/LayoutTestSupport.h"
#include "platform/Logging.h"
#include "platform/heap/Heap.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"
#include "wtf/Assertions.h"
#include "wtf/PtrUtil.h"
#include "wtf/WTF.h"
#include "wtf/allocator/Partitions.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/TextEncoding.h"
#include <memory>
#include <v8.h>

namespace blink {

namespace {

class EndOfTaskRunner : public WebThread::TaskObserver {
public:
    void willProcessTask() override
    {
        AnimationClock::notifyTaskStart();
    }
    void didProcessTask() override
    {
        Microtask::performCheckpoint(mainThreadIsolate());
        V8Initializer::reportRejectedPromisesOnMainThread();
    }
};

} // namespace

static WebThread::TaskObserver* s_endOfTaskRunner = nullptr;

static ModulesInitializer& modulesInitializer()
{
    DEFINE_STATIC_LOCAL(std::unique_ptr<ModulesInitializer>, initializer, (wrapUnique(new ModulesInitializer)));
    return *initializer;
}

void initialize(Platform* platform)
{
    Platform::initialize(platform);

    V8Initializer::initializeMainThread();

    modulesInitializer().initialize();

    // currentThread is null if we are running on a thread without a message loop.
    if (WebThread* currentThread = platform->currentThread()) {
        DCHECK(!s_endOfTaskRunner);
        s_endOfTaskRunner = new EndOfTaskRunner;
        currentThread->addTaskObserver(s_endOfTaskRunner);
    }
}

void shutdown()
{
    ThreadState::current()->cleanupMainThread();

    // currentThread() is null if we are running on a thread without a message loop.
    if (Platform::current()->currentThread()) {
        // We don't need to (cannot) remove s_endOfTaskRunner from the current
        // message loop, because the message loop is already destructed before
        // the shutdown() is called.
        delete s_endOfTaskRunner;
        s_endOfTaskRunner = nullptr;
    }

    modulesInitializer().shutdown();

    V8Initializer::shutdownMainThread();

    Platform::shutdown();
}

v8::Isolate* mainThreadIsolate()
{
    return V8PerIsolateData::mainThreadIsolate();
}

// TODO(tkent): The following functions to wrap LayoutTestSupport should be
// moved to public/platform/.

void setLayoutTestMode(bool value)
{
    LayoutTestSupport::setIsRunningLayoutTest(value);
}

bool layoutTestMode()
{
    return LayoutTestSupport::isRunningLayoutTest();
}

void setMockThemeEnabledForTest(bool value)
{
    LayoutTestSupport::setMockThemeEnabledForTest(value);
}

void setFontAntialiasingEnabledForTest(bool value)
{
    LayoutTestSupport::setFontAntialiasingEnabledForTest(value);
}

bool fontAntialiasingEnabledForTest()
{
    return LayoutTestSupport::isFontAntialiasingEnabledForTest();
}

void setAlwaysUseComplexTextForTest(bool value)
{
    LayoutTestSupport::setAlwaysUseComplexTextForTest(value);
}

bool alwaysUseComplexTextForTest()
{
    return LayoutTestSupport::alwaysUseComplexTextForTest();
}

void enableLogChannel(const char* name)
{
#if !LOG_DISABLED
    WTFLogChannel* channel = getChannelFromName(name);
    if (channel)
        channel->state = WTFLogChannelOn;
#endif // !LOG_DISABLED
}

void resetPluginCache(bool reloadPages)
{
    DCHECK(!reloadPages);
    Page::refreshPlugins();
}

void decommitFreeableMemory()
{
    WTF::Partitions::decommitFreeableMemory();
}

void MemoryPressureNotificationToWorkerThreadIsolates(
    v8::MemoryPressureLevel level)
{
    WorkerBackingThread::
        MemoryPressureNotificationToWorkerThreadIsolates(level);
}

void setRAILModeOnWorkerThreadIsolates(v8::RAILMode railMode)
{
    WorkerBackingThread::setRAILModeOnWorkerThreadIsolates(railMode);
}

} // namespace blink
