/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef WorkerThread_h
#define WorkerThread_h

#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/workers/WorkerRunLoop.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/Forward.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"

namespace blink {
class WebWaitableEvent;
}

namespace WebCore {

    class KURL;
    class NotificationClient;
    class WorkerGlobalScope;
    class WorkerLoaderProxy;
    class WorkerReportingProxy;
    class WorkerThreadStartupData;

    enum WorkerThreadStartMode { DontPauseWorkerGlobalScopeOnStart, PauseWorkerGlobalScopeOnStart };

    class WorkerThread : public RefCounted<WorkerThread> {
    public:
        virtual ~WorkerThread();

        bool start();
        void stop();

        // Can be used to wait for this worker thread to shut down.
        // (This is signalled on the main thread, so it's assumed to be waited on the worker context thread)
        blink::WebWaitableEvent* shutdownEvent() { return m_shutdownEvent.get(); }

        bool isCurrentThread() const;
        WorkerRunLoop& runLoop() { return m_runLoop; }
        WorkerLoaderProxy& workerLoaderProxy() const { return m_workerLoaderProxy; }
        WorkerReportingProxy& workerReportingProxy() const { return m_workerReportingProxy; }

        // Number of active worker threads.
        static unsigned workerThreadCount();

        NotificationClient* getNotificationClient() { return m_notificationClient; }
        void setNotificationClient(NotificationClient* client) { m_notificationClient = client; }

    protected:
        WorkerThread(WorkerLoaderProxy&, WorkerReportingProxy&, PassOwnPtrWillBeRawPtr<WorkerThreadStartupData>);

        // Factory method for creating a new worker context for the thread.
        virtual PassRefPtrWillBeRawPtr<WorkerGlobalScope> createWorkerGlobalScope(PassOwnPtrWillBeRawPtr<WorkerThreadStartupData>) = 0;

        // Executes the event loop for the worker thread. Derived classes can override to perform actions before/after entering the event loop.
        virtual void runEventLoop();

        WorkerGlobalScope* workerGlobalScope() { return m_workerGlobalScope.get(); }

    private:
        // Static function executed as the core routine on the new thread. Passed a pointer to a WorkerThread object.
        static void workerThreadStart(void*);

        void workerThread();

        ThreadIdentifier m_threadID;
        WorkerRunLoop m_runLoop;
        WorkerLoaderProxy& m_workerLoaderProxy;
        WorkerReportingProxy& m_workerReportingProxy;

        RefPtrWillBePersistent<WorkerGlobalScope> m_workerGlobalScope;
        Mutex m_threadCreationMutex;

        OwnPtrWillBePersistent<WorkerThreadStartupData> m_startupData;

        NotificationClient* m_notificationClient;

        // Used to signal thread shutdown.
        OwnPtr<blink::WebWaitableEvent> m_shutdownEvent;
    };

} // namespace WebCore

#endif // WorkerThread_h
