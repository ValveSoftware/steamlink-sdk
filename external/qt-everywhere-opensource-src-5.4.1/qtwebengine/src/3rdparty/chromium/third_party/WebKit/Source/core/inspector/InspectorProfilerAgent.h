/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InspectorProfilerAgent_h
#define InspectorProfilerAgent_h

#include "core/InspectorFrontend.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class InjectedScriptManager;
class InspectorFrontend;
class InspectorOverlay;
class InstrumentingAgents;
class ScriptState;
class ScriptCallStack;
class ScriptProfile;


typedef String ErrorString;

class InspectorProfilerAgent FINAL : public InspectorBaseAgent<InspectorProfilerAgent>, public InspectorBackendDispatcher::ProfilerCommandHandler {
    WTF_MAKE_NONCOPYABLE(InspectorProfilerAgent); WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<InspectorProfilerAgent> create(InjectedScriptManager*, InspectorOverlay*);
    virtual ~InspectorProfilerAgent();

    void consoleProfile(const String& title, ScriptState*);
    void consoleProfileEnd(const String& title, ScriptState*);

    virtual void enable(ErrorString*) OVERRIDE;
    virtual void disable(ErrorString*) OVERRIDE;
    virtual void setSamplingInterval(ErrorString*, int) OVERRIDE;
    virtual void start(ErrorString*) OVERRIDE;
    virtual void stop(ErrorString*, RefPtr<TypeBuilder::Profiler::CPUProfile>&) OVERRIDE;

    virtual void setFrontend(InspectorFrontend*) OVERRIDE;
    virtual void clearFrontend() OVERRIDE;
    virtual void restore() OVERRIDE;

    void willProcessTask();
    void didProcessTask();
    void willEnterNestedRunLoop();
    void didLeaveNestedRunLoop();

private:
    InspectorProfilerAgent(InjectedScriptManager*, InspectorOverlay*);
    bool enabled();
    void doEnable();
    void stop(ErrorString*, RefPtr<TypeBuilder::Profiler::CPUProfile>*);
    String nextProfileId();

    InjectedScriptManager* m_injectedScriptManager;
    InspectorFrontend::Profiler* m_frontend;
    bool m_recordingCPUProfile;
    class ProfileDescriptor;
    Vector<ProfileDescriptor> m_startedProfiles;
    String m_frontendInitiatedProfileId;

    typedef HashMap<String, double> ProfileNameIdleTimeMap;
    ProfileNameIdleTimeMap* m_profileNameIdleTimeMap;
    double m_idleStartTime;
    InspectorOverlay* m_overlay;

    void idleStarted();
    void idleFinished();
};

} // namespace WebCore


#endif // !defined(InspectorProfilerAgent_h)
