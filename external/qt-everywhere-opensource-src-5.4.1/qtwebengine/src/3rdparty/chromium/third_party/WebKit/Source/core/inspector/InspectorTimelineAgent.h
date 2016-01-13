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

#ifndef InspectorTimelineAgent_h
#define InspectorTimelineAgent_h


#include "bindings/v8/ScriptGCEvent.h"
#include "core/InspectorFrontend.h"
#include "core/InspectorTypeBuilder.h"
#include "core/events/EventPath.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/ScriptGCEventListener.h"
#include "core/inspector/TraceEventDispatcher.h"
#include "platform/JSONValues.h"
#include "platform/PlatformInstrumentation.h"
#include "platform/geometry/LayoutRect.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/Vector.h"
#include "wtf/WeakPtr.h"

namespace WebCore {
struct FetchInitiatorInfo;
struct TimelineImageInfo;
struct TimelineThreadState;
struct TimelineRecordEntry;

class LocalDOMWindow;
class Document;
class DocumentLoader;
class Event;
class ExecutionContext;
class FloatQuad;
class LocalFrame;
class FrameHost;
class GraphicsContext;
class GraphicsLayer;
class InspectorClient;
class InspectorFrontend;
class InspectorOverlay;
class InspectorPageAgent;
class InspectorLayerTreeAgent;
class InstrumentingAgents;
class KURL;
class ScriptState;
class Node;
class RenderImage;
class RenderObject;
class ResourceError;
class ResourceLoader;
class ResourceRequest;
class ResourceResponse;
class ScriptArguments;
class ScriptCallStack;
class TimelineRecordStack;
class WebSocketHandshakeRequest;
class WebSocketHandshakeResponse;
class XMLHttpRequest;

typedef String ErrorString;

class InspectorTimelineAgent FINAL
    : public TraceEventTarget<InspectorTimelineAgent>
    , public InspectorBaseAgent<InspectorTimelineAgent>
    , public ScriptGCEventListener
    , public InspectorBackendDispatcher::TimelineCommandHandler
    , public PlatformInstrumentationClient {
    WTF_MAKE_NONCOPYABLE(InspectorTimelineAgent);
public:
    enum InspectorType { PageInspector, WorkerInspector };

    class GPUEvent {
    public:
        enum Phase { PhaseBegin, PhaseEnd };
        GPUEvent(double timestamp, int phase, bool foreign, uint64_t usedGPUMemoryBytes, uint64_t limitGPUMemoryBytes) :
            timestamp(timestamp),
            phase(static_cast<Phase>(phase)),
            foreign(foreign),
            usedGPUMemoryBytes(usedGPUMemoryBytes),
            limitGPUMemoryBytes(limitGPUMemoryBytes) { }
        double timestamp;
        Phase phase;
        bool foreign;
        uint64_t usedGPUMemoryBytes;
        uint64_t limitGPUMemoryBytes;
    };

    static PassOwnPtr<InspectorTimelineAgent> create(InspectorPageAgent* pageAgent, InspectorLayerTreeAgent* layerTreeAgent,
        InspectorOverlay* overlay, InspectorType type, InspectorClient* client)
    {
        return adoptPtr(new InspectorTimelineAgent(pageAgent, layerTreeAgent, overlay, type, client));
    }

    virtual ~InspectorTimelineAgent();

    virtual void setFrontend(InspectorFrontend*) OVERRIDE;
    virtual void clearFrontend() OVERRIDE;
    virtual void restore() OVERRIDE;

    virtual void enable(ErrorString*) OVERRIDE;
    virtual void disable(ErrorString*) OVERRIDE;
    virtual void start(ErrorString*, const int* maxCallStackDepth, const bool* bufferEvents, const String* liveEvents, const bool* includeCounters, const bool* includeGPUEvents) OVERRIDE;
    virtual void stop(ErrorString*, RefPtr<TypeBuilder::Array<TypeBuilder::Timeline::TimelineEvent> >& events) OVERRIDE;

    void setLayerTreeId(int layerTreeId) { m_layerTreeId = layerTreeId; }
    int id() const { return m_id; }

    void didCommitLoad();

    // Methods called from WebCore.
    bool willCallFunction(ExecutionContext*, int scriptId, const String& scriptName, int scriptLine);
    void didCallFunction();

    bool willDispatchEvent(Document* document, const Event& event, LocalDOMWindow* window, Node* node, const EventPath& eventPath);
    bool willDispatchEventOnWindow(const Event& event, LocalDOMWindow* window);
    void didDispatchEvent();
    void didDispatchEventOnWindow();

    void didBeginFrame(int frameId);
    void didCancelFrame();

    void didInvalidateLayout(LocalFrame*);
    bool willLayout(LocalFrame*);
    void didLayout(RenderObject*);

    void willUpdateLayerTree();
    void layerTreeDidChange();
    void didUpdateLayerTree();

    void didScheduleStyleRecalculation(Document*);
    bool willRecalculateStyle(Document*);
    void didRecalculateStyle(int elementCount);

    void willPaint(RenderObject*, const GraphicsLayer*);
    void didPaint(RenderObject*, const GraphicsLayer*, GraphicsContext*, const LayoutRect&);

    void willPaintImage(RenderImage*);
    void didPaintImage();

    void willScrollLayer(RenderObject*);
    void didScrollLayer();

    void willComposite();
    void didComposite();

    bool willWriteHTML(Document*, unsigned startLine);
    void didWriteHTML(unsigned endLine);

    void didInstallTimer(ExecutionContext*, int timerId, int timeout, bool singleShot);
    void didRemoveTimer(ExecutionContext*, int timerId);
    bool willFireTimer(ExecutionContext*, int timerId);
    void didFireTimer();

    bool willDispatchXHRReadyStateChangeEvent(ExecutionContext*, XMLHttpRequest*);
    void didDispatchXHRReadyStateChangeEvent();
    bool willDispatchXHRLoadEvent(ExecutionContext*, XMLHttpRequest*);
    void didDispatchXHRLoadEvent();

    bool willEvaluateScript(LocalFrame*, const String&, int);
    void didEvaluateScript();

    void consoleTimeStamp(ExecutionContext*, const String& title);
    void domContentLoadedEventFired(LocalFrame*);
    void loadEventFired(LocalFrame*);

    void consoleTime(ExecutionContext*, const String&);
    void consoleTimeEnd(ExecutionContext*, const String&, ScriptState*);
    void consoleTimeline(ExecutionContext*, const String& title, ScriptState*);
    void consoleTimelineEnd(ExecutionContext*, const String& title, ScriptState*);

    void willSendRequest(unsigned long, DocumentLoader*, const ResourceRequest&, const ResourceResponse&, const FetchInitiatorInfo&);
    void didReceiveResourceResponse(LocalFrame*, unsigned long, DocumentLoader*, const ResourceResponse&, ResourceLoader*);
    void didFinishLoading(unsigned long, DocumentLoader*, double monotonicFinishTime, int64_t);
    void didFailLoading(unsigned long identifier, const ResourceError&);
    void didReceiveData(LocalFrame*, unsigned long identifier, const char* data, int dataLength, int encodedDataLength);

    void didRequestAnimationFrame(Document*, int callbackId);
    void didCancelAnimationFrame(Document*, int callbackId);
    bool willFireAnimationFrame(Document*, int callbackId);
    void didFireAnimationFrame();

    void willProcessTask();
    void didProcessTask();

    void didCreateWebSocket(Document*, unsigned long identifier, const KURL&, const String& protocol);
    void willSendWebSocketHandshakeRequest(Document*, unsigned long identifier, const WebSocketHandshakeRequest*);
    void didReceiveWebSocketHandshakeResponse(Document*, unsigned long identifier, const WebSocketHandshakeRequest*, const WebSocketHandshakeResponse*);
    void didCloseWebSocket(Document*, unsigned long identifier);

    void processGPUEvent(const GPUEvent&);

    // ScriptGCEventListener methods.
    virtual void didGC(double, double, size_t) OVERRIDE;

    // PlatformInstrumentationClient methods.
    virtual void willDecodeImage(const String& imageType) OVERRIDE;
    virtual void didDecodeImage() OVERRIDE;
    virtual void willResizeImage(bool shouldCache) OVERRIDE;
    virtual void didResizeImage() OVERRIDE;

private:

    friend class TimelineRecordStack;

    InspectorTimelineAgent(InspectorPageAgent*, InspectorLayerTreeAgent*, InspectorOverlay*, InspectorType, InspectorClient*);

    // Trace event handlers
    void onBeginImplSideFrame(const TraceEventDispatcher::TraceEvent&);
    void onPaintSetupBegin(const TraceEventDispatcher::TraceEvent&);
    void onPaintSetupEnd(const TraceEventDispatcher::TraceEvent&);
    void onRasterTaskBegin(const TraceEventDispatcher::TraceEvent&);
    void onRasterTaskEnd(const TraceEventDispatcher::TraceEvent&);
    void onImageDecodeBegin(const TraceEventDispatcher::TraceEvent&);
    void onImageDecodeEnd(const TraceEventDispatcher::TraceEvent&);
    void onLayerDeleted(const TraceEventDispatcher::TraceEvent&);
    void onDrawLazyPixelRef(const TraceEventDispatcher::TraceEvent&);
    void onDecodeLazyPixelRefBegin(const TraceEventDispatcher::TraceEvent&);
    void onDecodeLazyPixelRefEnd(const TraceEventDispatcher::TraceEvent&);
    void onRequestMainThreadFrame(const TraceEventDispatcher::TraceEvent&);
    void onActivateLayerTree(const TraceEventDispatcher::TraceEvent&);
    void onDrawFrame(const TraceEventDispatcher::TraceEvent&);
    void onLazyPixelRefDeleted(const TraceEventDispatcher::TraceEvent&);
    void onEmbedderCallbackBegin(const TraceEventDispatcher::TraceEvent&);
    void onEmbedderCallbackEnd(const TraceEventDispatcher::TraceEvent&);

    void didFinishLoadingResource(unsigned long, bool didFail, double finishTime);

    void sendEvent(PassRefPtr<TypeBuilder::Timeline::TimelineEvent>);
    void appendRecord(PassRefPtr<JSONObject> data, const String& type, bool captureCallStack, LocalFrame*);
    void pushCurrentRecord(PassRefPtr<JSONObject> data, const String& type, bool captureCallStack, LocalFrame*, bool hasLowLevelDetails = false);
    TimelineThreadState& threadState(ThreadIdentifier);

    void setFrameIdentifier(TypeBuilder::Timeline::TimelineEvent* record, LocalFrame*);
    void populateImageDetails(JSONObject* data, const RenderImage&);

    void didCompleteCurrentRecord(const String& type);
    void unwindRecordStack();

    void commitFrameRecord();

    void addRecordToTimeline(PassRefPtr<TypeBuilder::Timeline::TimelineEvent>, double ts);
    void innerAddRecordToTimeline(PassRefPtr<TypeBuilder::Timeline::TimelineEvent>);
    PassRefPtr<TypeBuilder::Timeline::TimelineEvent> createCountersUpdate();
    void clearRecordStack();
    PassRefPtr<TypeBuilder::Timeline::TimelineEvent> createRecordForEvent(const TraceEventDispatcher::TraceEvent&, const String& type, PassRefPtr<JSONObject> data);

    void localToPageQuad(const RenderObject& renderer, const LayoutRect&, FloatQuad*);
    long long nodeId(Node*);
    long long nodeId(RenderObject*);

    double timestamp();

    LocalFrame* mainFrame() const;

    bool isStarted();
    void innerStart();
    void innerStop(bool fromConsole);
    void setLiveEvents(const String&);

    InspectorPageAgent* m_pageAgent;
    InspectorLayerTreeAgent* m_layerTreeAgent;
    InspectorFrontend::Timeline* m_frontend;
    InspectorClient* m_client;
    InspectorOverlay* m_overlay;
    InspectorType m_inspectorType;

    int m_id;
    unsigned long long m_layerTreeId;

    int m_maxCallStackDepth;

    Vector<TimelineRecordEntry> m_recordStack;
    RefPtr<TypeBuilder::Array<TypeBuilder::Timeline::TimelineEvent> > m_bufferedEvents;
    Vector<String> m_consoleTimelines;

    unsigned m_platformInstrumentationClientInstalledAtStackDepth;
    RefPtr<TypeBuilder::Timeline::TimelineEvent> m_pendingFrameRecord;
    RefPtr<TypeBuilder::Timeline::TimelineEvent> m_pendingGPURecord;
    typedef HashMap<unsigned long long, TimelineImageInfo> PixelRefToImageInfoMap;
    PixelRefToImageInfoMap m_pixelRefToImageInfo;
    RenderImage* m_imageBeingPainted;
    HashMap<unsigned long long, long long> m_layerToNodeMap;
    double m_paintSetupStart;
    double m_paintSetupEnd;
    RefPtr<JSONObject> m_gpuTask;
    RefPtr<JSONValue> m_pendingLayerTreeData;
    typedef HashMap<ThreadIdentifier, TimelineThreadState> ThreadStateMap;
    ThreadStateMap m_threadStates;
    bool m_mayEmitFirstPaint;
    HashSet<String> m_liveEvents;
    double m_lastProgressTimestamp;
};

} // namespace WebCore

#endif // !defined(InspectorTimelineAgent_h)
