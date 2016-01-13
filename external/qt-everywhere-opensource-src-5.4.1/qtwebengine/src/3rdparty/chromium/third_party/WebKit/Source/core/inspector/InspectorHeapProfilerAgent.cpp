/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include "core/inspector/InspectorHeapProfilerAgent.h"

#include "bindings/v8/ScriptProfiler.h"
#include "core/inspector/InjectedScript.h"
#include "core/inspector/InjectedScriptHost.h"
#include "core/inspector/InspectorState.h"
#include "platform/Timer.h"
#include "wtf/CurrentTime.h"

namespace WebCore {

typedef uint32_t SnapshotObjectId;

namespace HeapProfilerAgentState {
static const char heapProfilerEnabled[] = "heapProfilerEnabled";
static const char heapObjectsTrackingEnabled[] = "heapObjectsTrackingEnabled";
static const char allocationTrackingEnabled[] = "allocationTrackingEnabled";
}

class InspectorHeapProfilerAgent::HeapStatsUpdateTask {
public:
    HeapStatsUpdateTask(InspectorHeapProfilerAgent*);
    void startTimer();
    void resetTimer() { m_timer.stop(); }
    void onTimer(Timer<HeapStatsUpdateTask>*);

private:
    InspectorHeapProfilerAgent* m_heapProfilerAgent;
    Timer<HeapStatsUpdateTask> m_timer;
};

PassOwnPtr<InspectorHeapProfilerAgent> InspectorHeapProfilerAgent::create(InjectedScriptManager* injectedScriptManager)
{
    return adoptPtr(new InspectorHeapProfilerAgent(injectedScriptManager));
}

InspectorHeapProfilerAgent::InspectorHeapProfilerAgent(InjectedScriptManager* injectedScriptManager)
    : InspectorBaseAgent<InspectorHeapProfilerAgent>("HeapProfiler")
    , m_injectedScriptManager(injectedScriptManager)
    , m_frontend(0)
    , m_nextUserInitiatedHeapSnapshotNumber(1)
{
}

InspectorHeapProfilerAgent::~InspectorHeapProfilerAgent()
{
}

void InspectorHeapProfilerAgent::setFrontend(InspectorFrontend* frontend)
{
    m_frontend = frontend->heapprofiler();
}

void InspectorHeapProfilerAgent::clearFrontend()
{
    m_frontend = 0;

    m_nextUserInitiatedHeapSnapshotNumber = 1;
    stopTrackingHeapObjectsInternal();
    m_injectedScriptManager->injectedScriptHost()->clearInspectedObjects();

    ErrorString error;
    disable(&error);
}

void InspectorHeapProfilerAgent::restore()
{
    if (m_state->getBoolean(HeapProfilerAgentState::heapProfilerEnabled))
        m_frontend->resetProfiles();
    if (m_state->getBoolean(HeapProfilerAgentState::heapObjectsTrackingEnabled))
        startTrackingHeapObjectsInternal(m_state->getBoolean(HeapProfilerAgentState::allocationTrackingEnabled));
}

void InspectorHeapProfilerAgent::collectGarbage(WebCore::ErrorString*)
{
    ScriptProfiler::collectGarbage();
}

InspectorHeapProfilerAgent::HeapStatsUpdateTask::HeapStatsUpdateTask(InspectorHeapProfilerAgent* heapProfilerAgent)
    : m_heapProfilerAgent(heapProfilerAgent)
    , m_timer(this, &HeapStatsUpdateTask::onTimer)
{
}

void InspectorHeapProfilerAgent::HeapStatsUpdateTask::onTimer(Timer<HeapStatsUpdateTask>*)
{
    // The timer is stopped on m_heapProfilerAgent destruction,
    // so this method will never be called after m_heapProfilerAgent has been destroyed.
    m_heapProfilerAgent->requestHeapStatsUpdate();
}

void InspectorHeapProfilerAgent::HeapStatsUpdateTask::startTimer()
{
    ASSERT(!m_timer.isActive());
    m_timer.startRepeating(0.05, FROM_HERE);
}

class InspectorHeapProfilerAgent::HeapStatsStream FINAL : public ScriptProfiler::OutputStream {
public:
    HeapStatsStream(InspectorHeapProfilerAgent* heapProfilerAgent)
        : m_heapProfilerAgent(heapProfilerAgent)
    {
    }

    virtual void write(const uint32_t* chunk, const int size) OVERRIDE
    {
        ASSERT(chunk);
        ASSERT(size > 0);
        m_heapProfilerAgent->pushHeapStatsUpdate(chunk, size);
    }
private:
    InspectorHeapProfilerAgent* m_heapProfilerAgent;
};

void InspectorHeapProfilerAgent::startTrackingHeapObjects(ErrorString*, const bool* trackAllocations)
{
    m_state->setBoolean(HeapProfilerAgentState::heapObjectsTrackingEnabled, true);
    bool allocationTrackingEnabled = trackAllocations && *trackAllocations;
    m_state->setBoolean(HeapProfilerAgentState::allocationTrackingEnabled, allocationTrackingEnabled);
    startTrackingHeapObjectsInternal(allocationTrackingEnabled);
}

void InspectorHeapProfilerAgent::requestHeapStatsUpdate()
{
    if (!m_frontend)
        return;
    HeapStatsStream stream(this);
    SnapshotObjectId lastSeenObjectId = ScriptProfiler::requestHeapStatsUpdate(&stream);
    m_frontend->lastSeenObjectId(lastSeenObjectId, WTF::currentTimeMS());
}

void InspectorHeapProfilerAgent::pushHeapStatsUpdate(const uint32_t* const data, const int size)
{
    if (!m_frontend)
        return;
    RefPtr<TypeBuilder::Array<int> > statsDiff = TypeBuilder::Array<int>::create();
    for (int i = 0; i < size; ++i)
        statsDiff->addItem(data[i]);
    m_frontend->heapStatsUpdate(statsDiff.release());
}

void InspectorHeapProfilerAgent::stopTrackingHeapObjects(ErrorString* error, const bool* reportProgress)
{
    if (!m_heapStatsUpdateTask) {
        *error = "Heap object tracking is not started.";
        return;
    }
    requestHeapStatsUpdate();
    takeHeapSnapshot(error, reportProgress);
    stopTrackingHeapObjectsInternal();
}

void InspectorHeapProfilerAgent::startTrackingHeapObjectsInternal(bool trackAllocations)
{
    if (m_heapStatsUpdateTask)
        return;
    ScriptProfiler::startTrackingHeapObjects(trackAllocations);
    m_heapStatsUpdateTask = adoptPtr(new HeapStatsUpdateTask(this));
    m_heapStatsUpdateTask->startTimer();
}

void InspectorHeapProfilerAgent::stopTrackingHeapObjectsInternal()
{
    if (!m_heapStatsUpdateTask)
        return;
    ScriptProfiler::stopTrackingHeapObjects();
    m_heapStatsUpdateTask->resetTimer();
    m_heapStatsUpdateTask.clear();
    m_state->setBoolean(HeapProfilerAgentState::heapObjectsTrackingEnabled, false);
    m_state->setBoolean(HeapProfilerAgentState::allocationTrackingEnabled, false);
}

void InspectorHeapProfilerAgent::enable(ErrorString*)
{
    m_state->setBoolean(HeapProfilerAgentState::heapProfilerEnabled, true);
}

void InspectorHeapProfilerAgent::disable(ErrorString* error)
{
    stopTrackingHeapObjectsInternal();
    ScriptProfiler::clearHeapObjectIds();
    m_state->setBoolean(HeapProfilerAgentState::heapProfilerEnabled, false);
}

void InspectorHeapProfilerAgent::takeHeapSnapshot(ErrorString* errorString, const bool* reportProgress)
{
    class HeapSnapshotProgress FINAL : public ScriptProfiler::HeapSnapshotProgress {
    public:
        explicit HeapSnapshotProgress(InspectorFrontend::HeapProfiler* frontend)
            : m_frontend(frontend) { }
        virtual void Start(int totalWork) OVERRIDE
        {
            m_totalWork = totalWork;
        }
        virtual void Worked(int workDone) OVERRIDE
        {
            if (m_frontend) {
                m_frontend->reportHeapSnapshotProgress(workDone, m_totalWork, 0);
                m_frontend->flush();
            }
        }
        virtual void Done() OVERRIDE
        {
            const bool finished = true;
            if (m_frontend) {
                m_frontend->reportHeapSnapshotProgress(m_totalWork, m_totalWork, &finished);
                m_frontend->flush();
            }
        }
        virtual bool isCanceled() OVERRIDE { return false; }
    private:
        InspectorFrontend::HeapProfiler* m_frontend;
        int m_totalWork;
    };

    String title = "Snapshot " + String::number(m_nextUserInitiatedHeapSnapshotNumber++);
    HeapSnapshotProgress progress(reportProgress && *reportProgress ? m_frontend : 0);
    RefPtr<ScriptHeapSnapshot> snapshot = ScriptProfiler::takeHeapSnapshot(title, &progress);
    if (!snapshot) {
        *errorString = "Failed to take heap snapshot";
        return;
    }

    class OutputStream : public ScriptHeapSnapshot::OutputStream {
    public:
        explicit OutputStream(InspectorFrontend::HeapProfiler* frontend)
            : m_frontend(frontend) { }
        void Write(const String& chunk)
        {
            m_frontend->addHeapSnapshotChunk(chunk);
            m_frontend->flush();
        }
        void Close() { }
    private:
        InspectorFrontend::HeapProfiler* m_frontend;
    };

    if (m_frontend) {
        OutputStream stream(m_frontend);
        snapshot->writeJSON(&stream);
    }
}

void InspectorHeapProfilerAgent::getObjectByHeapObjectId(ErrorString* error, const String& heapSnapshotObjectId, const String* objectGroup, RefPtr<TypeBuilder::Runtime::RemoteObject>& result)
{
    bool ok;
    unsigned id = heapSnapshotObjectId.toUInt(&ok);
    if (!ok) {
        *error = "Invalid heap snapshot object id";
        return;
    }
    ScriptValue heapObject = ScriptProfiler::objectByHeapObjectId(id);
    if (heapObject.isEmpty()) {
        *error = "Object is not available";
        return;
    }
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptFor(heapObject.scriptState());
    if (injectedScript.isEmpty()) {
        *error = "Object is not available. Inspected context is gone";
        return;
    }
    result = injectedScript.wrapObject(heapObject, objectGroup ? *objectGroup : "");
    if (!result)
        *error = "Failed to wrap object";
}

void InspectorHeapProfilerAgent::getHeapObjectId(ErrorString* errorString, const String& objectId, String* heapSnapshotObjectId)
{
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptForObjectId(objectId);
    if (injectedScript.isEmpty()) {
        *errorString = "Inspected context has gone";
        return;
    }
    ScriptValue value = injectedScript.findObjectById(objectId);
    ScriptState::Scope scope(injectedScript.scriptState());
    if (value.isEmpty() || value.isUndefined()) {
        *errorString = "Object with given id not found";
        return;
    }
    unsigned id = ScriptProfiler::getHeapObjectId(value);
    *heapSnapshotObjectId = String::number(id);
}

} // namespace WebCore

