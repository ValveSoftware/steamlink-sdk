/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef InspectorCanvasAgent_h
#define InspectorCanvasAgent_h


#include "bindings/v8/ScriptState.h"
#include "core/InspectorFrontend.h"
#include "core/InspectorTypeBuilder.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "wtf/HashMap.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class LocalFrame;
class DocumentLoader;
class InjectedScriptCanvasModule;
class InjectedScriptManager;
class InspectorPageAgent;
class InstrumentingAgents;
class ScriptValue;

typedef String ErrorString;

class InspectorCanvasAgent FINAL : public InspectorBaseAgent<InspectorCanvasAgent>, public InspectorBackendDispatcher::CanvasCommandHandler {
public:
    static PassOwnPtr<InspectorCanvasAgent> create(InspectorPageAgent* pageAgent, InjectedScriptManager* injectedScriptManager)
    {
        return adoptPtr(new InspectorCanvasAgent(pageAgent, injectedScriptManager));
    }
    virtual ~InspectorCanvasAgent();

    virtual void setFrontend(InspectorFrontend*) OVERRIDE;
    virtual void clearFrontend() OVERRIDE;
    virtual void restore() OVERRIDE;

    void didCommitLoad(LocalFrame*, DocumentLoader*);
    void frameDetachedFromParent(LocalFrame*);
    void didBeginFrame();

    // Called from InspectorCanvasInstrumentation.
    ScriptValue wrapCanvas2DRenderingContextForInstrumentation(const ScriptValue&);
    ScriptValue wrapWebGLRenderingContextForInstrumentation(const ScriptValue&);

    // Called from the front-end.
    virtual void enable(ErrorString*) OVERRIDE;
    virtual void disable(ErrorString*) OVERRIDE;
    virtual void dropTraceLog(ErrorString*, const TypeBuilder::Canvas::TraceLogId&) OVERRIDE;
    virtual void hasUninstrumentedCanvases(ErrorString*, bool*) OVERRIDE;
    virtual void captureFrame(ErrorString*, const TypeBuilder::Page::FrameId*, TypeBuilder::Canvas::TraceLogId*) OVERRIDE;
    virtual void startCapturing(ErrorString*, const TypeBuilder::Page::FrameId*, TypeBuilder::Canvas::TraceLogId*) OVERRIDE;
    virtual void stopCapturing(ErrorString*, const TypeBuilder::Canvas::TraceLogId&) OVERRIDE;
    virtual void getTraceLog(ErrorString*, const TypeBuilder::Canvas::TraceLogId&, const int*, const int*, RefPtr<TypeBuilder::Canvas::TraceLog>&) OVERRIDE;
    virtual void replayTraceLog(ErrorString*, const TypeBuilder::Canvas::TraceLogId&, int, RefPtr<TypeBuilder::Canvas::ResourceState>&, double*) OVERRIDE;
    virtual void getResourceState(ErrorString*, const TypeBuilder::Canvas::TraceLogId&, const TypeBuilder::Canvas::ResourceId&, RefPtr<TypeBuilder::Canvas::ResourceState>&) OVERRIDE;
    virtual void evaluateTraceLogCallArgument(ErrorString*, const TypeBuilder::Canvas::TraceLogId&, int, int, const String*, RefPtr<TypeBuilder::Runtime::RemoteObject>&, RefPtr<TypeBuilder::Canvas::ResourceState>&) OVERRIDE;

private:
    InspectorCanvasAgent(InspectorPageAgent*, InjectedScriptManager*);

    InjectedScriptCanvasModule injectedScriptCanvasModule(ErrorString*, ScriptState*);
    InjectedScriptCanvasModule injectedScriptCanvasModule(ErrorString*, const ScriptValue&);
    InjectedScriptCanvasModule injectedScriptCanvasModule(ErrorString*, const String&);

    void findFramesWithUninstrumentedCanvases();
    bool checkIsEnabled(ErrorString*) const;
    ScriptValue notifyRenderingContextWasWrapped(const ScriptValue&);

    InspectorPageAgent* m_pageAgent;
    InjectedScriptManager* m_injectedScriptManager;
    InspectorFrontend::Canvas* m_frontend;
    bool m_enabled;
    // Contains all frames with canvases, value is true only for frames that have an uninstrumented canvas.
    typedef HashMap<LocalFrame*, bool> FramesWithUninstrumentedCanvases;
    FramesWithUninstrumentedCanvases m_framesWithUninstrumentedCanvases;
};

} // namespace WebCore


#endif // !defined(InspectorCanvasAgent_h)
