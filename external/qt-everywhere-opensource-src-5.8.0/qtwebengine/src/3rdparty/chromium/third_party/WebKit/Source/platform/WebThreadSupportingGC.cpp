// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/WebThreadSupportingGC.h"

#include "platform/heap/SafePoint.h"
#include "public/platform/WebScheduler.h"
#include "wtf/PtrUtil.h"
#include "wtf/Threading.h"
#include <memory>

namespace blink {

std::unique_ptr<WebThreadSupportingGC> WebThreadSupportingGC::create(const char* name, bool perThreadHeapEnabled)
{
    return wrapUnique(new WebThreadSupportingGC(name, nullptr, perThreadHeapEnabled));
}

std::unique_ptr<WebThreadSupportingGC> WebThreadSupportingGC::createForThread(WebThread* thread, bool perThreadHeapEnabled)
{
    return wrapUnique(new WebThreadSupportingGC(nullptr, thread, perThreadHeapEnabled));
}

WebThreadSupportingGC::WebThreadSupportingGC(const char* name, WebThread* thread, bool perThreadHeapEnabled)
    : m_thread(thread)
    , m_perThreadHeapEnabled(perThreadHeapEnabled)
{
#if ENABLE(ASSERT)
    ASSERT(!name || !thread);
    // We call this regardless of whether an existing thread is given or not,
    // as it means that blink is going to run with more than one thread.
    WTF::willCreateThread();
#endif
    if (!m_thread) {
        // If |thread| is not given, create a new one and own it.
        m_owningThread = wrapUnique(Platform::current()->createThread(name));
        m_thread = m_owningThread.get();
    }
}

WebThreadSupportingGC::~WebThreadSupportingGC()
{
    if (ThreadState::current() && m_owningThread) {
        // WebThread's destructor blocks until all the tasks are processed.
        SafePointScope scope(BlinkGC::HeapPointersOnStack);
        m_owningThread.reset();
    }
}

void WebThreadSupportingGC::initialize()
{
    ThreadState::attachCurrentThread(m_perThreadHeapEnabled);
    m_gcTaskRunner = wrapUnique(new GCTaskRunner(m_thread));
}

void WebThreadSupportingGC::shutdown()
{
#if defined(LEAK_SANITIZER)
    ThreadState::current()->releaseStaticPersistentNodes();
#endif
    // Ensure no posted tasks will run from this point on.
    m_gcTaskRunner.reset();

    // Shutdown the thread (via its scheduler) only when the thread is created
    // and is owned by this instance.
    if (m_owningThread)
        m_owningThread->scheduler()->shutdown();

    ThreadState::detachCurrentThread();
}

} // namespace blink
