// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerBackingThread_h
#define WorkerBackingThread_h

#include "core/CoreExport.h"
#include "wtf/Forward.h"
#include "wtf/PtrUtil.h"
#include "wtf/ThreadingPrimitives.h"
#include <memory>
#include <v8.h>

namespace blink {

class WebThread;
class WebThreadSupportingGC;

// WorkerBackingThread represents a WebThread with Oilpan and V8 potentially
// shared by multiple WebWorker scripts. A WebWorker needs to call initialize()
// to using V8 and Oilpan functionalities, and call detach() when the script
// no longer needs the thread.
// A WorkerBackingThread represents a WebThread while a WorkerThread corresponds
// to a web worker. There is one-to-one correspondence between WorkerThread and
// WorkerGlobalScope, but multiple WorkerThreads (i.e., multiple
// WorkerGlobalScopes) can share one WorkerBackingThread.
class CORE_EXPORT WorkerBackingThread final {
public:
    static std::unique_ptr<WorkerBackingThread> create(const char* name) { return wrapUnique(new WorkerBackingThread(name, false)); }
    static std::unique_ptr<WorkerBackingThread> create(WebThread* thread) { return wrapUnique(new WorkerBackingThread(thread, false)); }

    // These are needed to suppress leak reports. See
    // https://crbug.com/590802 and https://crbug.com/v8/1428.
    static std::unique_ptr<WorkerBackingThread> createForTest(const char* name) { return wrapUnique(new WorkerBackingThread(name, true)); }
    static std::unique_ptr<WorkerBackingThread> createForTest(WebThread* thread) { return wrapUnique(new WorkerBackingThread(thread, true)); }

    ~WorkerBackingThread();

    // initialize() and shutdown() attaches and detaches Oilpan and V8 to / from
    // the caller worker script, respectively. A worker script must not call
    // any function after calling shutdown().
    // They should be called from |this| thread.
    void initialize();
    void shutdown();

    WebThreadSupportingGC& backingThread()
    {
        DCHECK(m_backingThread);
        return *m_backingThread;
    }

    v8::Isolate* isolate() { return m_isolate; }

    static void MemoryPressureNotificationToWorkerThreadIsolates(
        v8::MemoryPressureLevel);

    static void setRAILModeOnWorkerThreadIsolates(v8::RAILMode);

private:
    WorkerBackingThread(const char* name, bool shouldCallGCOnShutdown);
    WorkerBackingThread(WebThread*, bool shouldCallGCOnSHutdown);

    std::unique_ptr<WebThreadSupportingGC> m_backingThread;
    v8::Isolate* m_isolate = nullptr;
    bool m_isOwningThread;
    bool m_shouldCallGCOnShutdown;
};

} // namespace blink

#endif // WorkerBackingThread_h
