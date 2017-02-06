// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/WorkerInspectorProxy.h"

#include "core/dom/CrossThreadTask.h"
#include "core/frame/FrameConsole.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/inspector/WorkerInspectorController.h"
#include "core/workers/WorkerThread.h"
#include "platform/TraceEvent.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebTaskRunner.h"
#include "public/platform/WebTraceLocation.h"

namespace blink {

namespace {

static const unsigned maxConsoleMessageCount = 1000;

static WorkerInspectorProxy::WorkerInspectorProxySet& inspectorProxies()
{
    DEFINE_STATIC_LOCAL(WorkerInspectorProxy::WorkerInspectorProxySet, proxies, ());
    return proxies;
}

} // namespace

const WorkerInspectorProxy::WorkerInspectorProxySet& WorkerInspectorProxy::allProxies()
{
    return inspectorProxies();
}

WorkerInspectorProxy::WorkerInspectorProxy()
    : m_workerThread(nullptr)
    , m_document(nullptr)
    , m_pageInspector(nullptr)
    , m_ignoreConsoleMessages(false)
{
}

WorkerInspectorProxy* WorkerInspectorProxy::create()
{
    return new WorkerInspectorProxy();
}

WorkerInspectorProxy::~WorkerInspectorProxy()
{
}

const String& WorkerInspectorProxy::inspectorId()
{
    if (m_inspectorId.isEmpty())
        m_inspectorId = "dedicated:" + IdentifiersFactory::createIdentifier();
    return m_inspectorId;
}

WorkerThreadStartMode WorkerInspectorProxy::workerStartMode(Document* document)
{
    if (InspectorInstrumentation::shouldWaitForDebuggerOnWorkerStart(document))
        return PauseWorkerGlobalScopeOnStart;
    return DontPauseWorkerGlobalScopeOnStart;
}

void WorkerInspectorProxy::workerThreadCreated(Document* document, WorkerThread* workerThread, const KURL& url)
{
    m_workerThread = workerThread;
    m_document = document;
    m_url = url.getString();
    inspectorProxies().add(this);
    // We expect everyone starting worker thread to synchronously ask for workerStartMode right before.
    bool waitingForDebugger = InspectorInstrumentation::shouldWaitForDebuggerOnWorkerStart(document);
    InspectorInstrumentation::didStartWorker(document, this, waitingForDebugger);
}

void WorkerInspectorProxy::workerThreadTerminated()
{
    if (m_workerThread) {
        DCHECK(inspectorProxies().contains(this));
        inspectorProxies().remove(this);
        InspectorInstrumentation::workerTerminated(m_document, this);
    }

    LocalFrame* frame = m_document ? m_document->frame() : nullptr;
    if (frame) {
        for (ConsoleMessage* message : m_consoleMessages)
            frame->console().adoptWorkerMessage(message);
        m_consoleMessages.clear();
    }

    m_workerThread = nullptr;
    m_pageInspector = nullptr;
    m_document = nullptr;
}

void WorkerInspectorProxy::dispatchMessageFromWorker(const String& message)
{
    if (m_pageInspector)
        m_pageInspector->dispatchMessageFromWorker(this, message);
}

void WorkerInspectorProxy::workerConsoleAgentEnabled()
{
    m_ignoreConsoleMessages = true;
    m_consoleMessages.clear();
}

void WorkerInspectorProxy::addConsoleMessageFromWorker(ConsoleMessage* consoleMessage)
{
    if (!m_ignoreConsoleMessages) {
        DCHECK(m_consoleMessages.size() <= maxConsoleMessageCount);
        if (m_consoleMessages.size() == maxConsoleMessageCount)
            m_consoleMessages.removeFirst();
        m_consoleMessages.append(consoleMessage);
    }
    if (LocalFrame* frame = m_document->frame())
        frame->console().reportWorkerMessage(consoleMessage);
}

static void connectToWorkerGlobalScopeInspectorTask(WorkerThread* workerThread)
{
    if (WorkerInspectorController* inspector = workerThread->workerGlobalScope()->workerInspectorController())
        inspector->connectFrontend();
}

void WorkerInspectorProxy::connectToInspector(WorkerInspectorProxy::PageInspector* pageInspector)
{
    if (!m_workerThread)
        return;
    DCHECK(!m_pageInspector);
    m_pageInspector = pageInspector;
    m_workerThread->appendDebuggerTask(crossThreadBind(connectToWorkerGlobalScopeInspectorTask, crossThreadUnretained(m_workerThread)));
}

static void disconnectFromWorkerGlobalScopeInspectorTask(WorkerThread* workerThread)
{
    if (WorkerInspectorController* inspector = workerThread->workerGlobalScope()->workerInspectorController())
        inspector->disconnectFrontend();
}

void WorkerInspectorProxy::disconnectFromInspector(WorkerInspectorProxy::PageInspector* pageInspector)
{
    DCHECK(m_pageInspector == pageInspector);
    m_pageInspector = nullptr;
    if (m_workerThread)
        m_workerThread->appendDebuggerTask(crossThreadBind(disconnectFromWorkerGlobalScopeInspectorTask, crossThreadUnretained(m_workerThread)));
}

static void dispatchOnInspectorBackendTask(const String& message, WorkerThread* workerThread)
{
    if (WorkerInspectorController* inspector = workerThread->workerGlobalScope()->workerInspectorController())
        inspector->dispatchMessageFromFrontend(message);
}

void WorkerInspectorProxy::sendMessageToInspector(const String& message)
{
    if (m_workerThread)
        m_workerThread->appendDebuggerTask(crossThreadBind(dispatchOnInspectorBackendTask, message, crossThreadUnretained(m_workerThread)));
}

void WorkerInspectorProxy::writeTimelineStartedEvent(const String& sessionId)
{
    if (!m_workerThread)
        return;
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "TracingSessionIdForWorker", TRACE_EVENT_SCOPE_THREAD, "data", InspectorTracingSessionIdForWorkerEvent::data(sessionId, inspectorId(), m_workerThread));
}

DEFINE_TRACE(WorkerInspectorProxy)
{
    visitor->trace(m_document);
    visitor->trace(m_consoleMessages);
}

} // namespace blink
