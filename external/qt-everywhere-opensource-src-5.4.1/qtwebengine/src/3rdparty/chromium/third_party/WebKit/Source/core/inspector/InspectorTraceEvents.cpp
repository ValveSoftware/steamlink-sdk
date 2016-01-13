// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/inspector/InspectorTraceEvents.h"

#include "bindings/v8/ScriptCallStackFactory.h"
#include "bindings/v8/ScriptGCEvent.h"
#include "bindings/v8/ScriptSourceCode.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectorNodeIds.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/page/Page.h"
#include "core/rendering/RenderImage.h"
#include "core/rendering/RenderObject.h"
#include "core/xml/XMLHttpRequest.h"
#include "platform/JSONValues.h"
#include "platform/TracedValue.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"
#include "platform/weborigin/KURL.h"
#include "wtf/Vector.h"
#include <inttypes.h>

namespace WebCore {

namespace {

class JSCallStack : public TraceEvent::ConvertableToTraceFormat  {
public:
    explicit JSCallStack(PassRefPtrWillBeRawPtr<ScriptCallStack> callstack) : m_callstack(callstack) { }
    virtual String asTraceFormat() const
    {
        if (!m_callstack)
            return "null";
        return m_callstack->buildInspectorArray()->toJSONString();
    }

private:
    RefPtrWillBePersistent<ScriptCallStack> m_callstack;
};

String toHexString(void* p)
{
    return String::format("0x%" PRIx64, static_cast<uint64>(reinterpret_cast<intptr_t>(p)));
}

}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorLayoutEvent::beginData(FrameView* frameView)
{
    bool isPartial;
    unsigned needsLayoutObjects;
    unsigned totalObjects;
    LocalFrame& frame = frameView->frame();
    frame.countObjectsNeedingLayout(needsLayoutObjects, totalObjects, isPartial);

    RefPtr<JSONObject> data = JSONObject::create();
    data->setNumber("dirtyObjects", needsLayoutObjects);
    data->setNumber("totalObjects", totalObjects);
    data->setBoolean("partialLayout", isPartial);
    data->setString("frame", toHexString(&frame));
    return TracedValue::fromJSONValue(data);
}

static PassRefPtr<JSONArray> createQuad(const FloatQuad& quad)
{
    RefPtr<JSONArray> array = JSONArray::create();
    array->pushNumber(quad.p1().x());
    array->pushNumber(quad.p1().y());
    array->pushNumber(quad.p2().x());
    array->pushNumber(quad.p2().y());
    array->pushNumber(quad.p3().x());
    array->pushNumber(quad.p3().y());
    array->pushNumber(quad.p4().x());
    array->pushNumber(quad.p4().y());
    return array.release();
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorLayoutEvent::endData(RenderObject* rootForThisLayout)
{
    Vector<FloatQuad> quads;
    rootForThisLayout->absoluteQuads(quads);

    RefPtr<JSONObject> data = JSONObject::create();
    if (quads.size() >= 1) {
        data->setArray("root", createQuad(quads[0]));
        int rootNodeId = InspectorNodeIds::idForNode(rootForThisLayout->generatingNode());
        data->setNumber("rootNode", rootNodeId);
    } else {
        ASSERT_NOT_REACHED();
    }
    return TracedValue::fromJSONValue(data);
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorSendRequestEvent::data(unsigned long identifier, LocalFrame* frame, const ResourceRequest& request)
{
    String requestId = IdentifiersFactory::requestId(identifier);

    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("requestId", requestId);
    data->setString("frame", toHexString(frame));
    data->setString("url", request.url().string());
    data->setString("requestMethod", request.httpMethod());
    return TracedValue::fromJSONValue(data);
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorReceiveResponseEvent::data(unsigned long identifier, LocalFrame* frame, const ResourceResponse& response)
{
    String requestId = IdentifiersFactory::requestId(identifier);

    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("requestId", requestId);
    data->setString("frame", toHexString(frame));
    data->setNumber("statusCode", response.httpStatusCode());
    data->setString("mimeType", response.mimeType().string().isolatedCopy());
    return TracedValue::fromJSONValue(data);
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorReceiveDataEvent::data(unsigned long identifier, LocalFrame* frame, int encodedDataLength)
{
    String requestId = IdentifiersFactory::requestId(identifier);

    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("requestId", requestId);
    data->setString("frame", toHexString(frame));
    data->setNumber("encodedDataLength", encodedDataLength);
    return TracedValue::fromJSONValue(data);
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorResourceFinishEvent::data(unsigned long identifier, double finishTime, bool didFail)
{
    String requestId = IdentifiersFactory::requestId(identifier);

    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("requestId", requestId);
    data->setBoolean("didFail", didFail);
    if (finishTime)
        data->setNumber("networkTime", finishTime);
    return TracedValue::fromJSONValue(data);
}

static LocalFrame* frameForExecutionContext(ExecutionContext* context)
{
    LocalFrame* frame = 0;
    if (context->isDocument())
        frame = toDocument(context)->frame();
    return frame;
}

static PassRefPtr<JSONObject> genericTimerData(ExecutionContext* context, int timerId)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setNumber("timerId", timerId);
    if (LocalFrame* frame = frameForExecutionContext(context))
        data->setString("frame", toHexString(frame));
    return data.release();
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorTimerInstallEvent::data(ExecutionContext* context, int timerId, int timeout, bool singleShot)
{
    RefPtr<JSONObject> data = genericTimerData(context, timerId);
    data->setNumber("timeout", timeout);
    data->setBoolean("singleShot", singleShot);
    return TracedValue::fromJSONValue(data);
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorTimerRemoveEvent::data(ExecutionContext* context, int timerId)
{
    return TracedValue::fromJSONValue(genericTimerData(context, timerId));
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorTimerFireEvent::data(ExecutionContext* context, int timerId)
{
    return TracedValue::fromJSONValue(genericTimerData(context, timerId));
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorAnimationFrameEvent::data(Document* document, int callbackId)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setNumber("id", callbackId);
    data->setString("frame", toHexString(document->frame()));
    return TracedValue::fromJSONValue(data);
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorWebSocketCreateEvent::data(Document* document, unsigned long identifier, const KURL& url, const String& protocol)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setNumber("identifier", identifier);
    data->setString("url", url.string());
    data->setString("frame", toHexString(document->frame()));
    if (!protocol.isNull())
        data->setString("webSocketProtocol", protocol);
    return TracedValue::fromJSONValue(data);
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorWebSocketEvent::data(Document* document, unsigned long identifier)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setNumber("identifier", identifier);
    data->setString("frame", toHexString(document->frame()));
    return TracedValue::fromJSONValue(data);
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorParseHtmlEvent::beginData(Document* document, unsigned startLine)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setNumber("startLine", startLine);
    data->setString("frame", toHexString(document->frame()));
    return TracedValue::fromJSONValue(data);
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorXhrReadyStateChangeEvent::data(ExecutionContext* context, XMLHttpRequest* request)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("url", request->url().string());
    data->setNumber("readyState", request->readyState());
    if (LocalFrame* frame = frameForExecutionContext(context))
        data->setString("frame", toHexString(frame));
    return TracedValue::fromJSONValue(data);
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorXhrLoadEvent::data(ExecutionContext* context, XMLHttpRequest* request)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("url", request->url().string());
    if (LocalFrame* frame = frameForExecutionContext(context))
        data->setString("frame", toHexString(frame));
    return TracedValue::fromJSONValue(data);
}

static void localToPageQuad(const RenderObject& renderer, const LayoutRect& rect, FloatQuad* quad)
{
    LocalFrame* frame = renderer.frame();
    FrameView* view = frame->view();
    FloatQuad absolute = renderer.localToAbsoluteQuad(FloatQuad(rect));
    quad->setP1(view->contentsToRootView(roundedIntPoint(absolute.p1())));
    quad->setP2(view->contentsToRootView(roundedIntPoint(absolute.p2())));
    quad->setP3(view->contentsToRootView(roundedIntPoint(absolute.p3())));
    quad->setP4(view->contentsToRootView(roundedIntPoint(absolute.p4())));
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorPaintEvent::data(RenderObject* renderer, const LayoutRect& clipRect, const GraphicsLayer* graphicsLayer)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("frame", toHexString(renderer->frame()));
    FloatQuad quad;
    localToPageQuad(*renderer, clipRect, &quad);
    data->setArray("clip", createQuad(quad));
    int nodeId = InspectorNodeIds::idForNode(renderer->generatingNode());
    data->setNumber("nodeId", nodeId);
    int graphicsLayerId = graphicsLayer ? graphicsLayer->platformLayer()->id() : 0;
    data->setNumber("layerId", graphicsLayerId);
    return TracedValue::fromJSONValue(data);
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorMarkLoadEvent::data(LocalFrame* frame)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("frame", toHexString(frame));
    bool isMainFrame = frame && frame->page()->mainFrame() == frame;
    data->setBoolean("isMainFrame", isMainFrame);
    return TracedValue::fromJSONValue(data);
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorScrollLayerEvent::data(RenderObject* renderer)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("frame", toHexString(renderer->frame()));
    int nodeId = InspectorNodeIds::idForNode(renderer->generatingNode());
    data->setNumber("nodeId", nodeId);
    return TracedValue::fromJSONValue(data);
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorEvaluateScriptEvent::data(LocalFrame* frame, const String& url, int lineNumber)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("frame", toHexString(frame));
    data->setString("url", url);
    data->setNumber("lineNumber", lineNumber);
    return TracedValue::fromJSONValue(data);
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorFunctionCallEvent::data(ExecutionContext* context, int scriptId, const String& scriptName, int scriptLine)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("scriptId", String::number(scriptId));
    data->setString("scriptName", scriptName);
    data->setNumber("scriptLine", scriptLine);
    if (LocalFrame* frame = frameForExecutionContext(context))
        data->setString("frame", toHexString(frame));
    return TracedValue::fromJSONValue(data);
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorPaintImageEvent::data(const RenderImage& renderImage)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setNumber("nodeId", InspectorNodeIds::idForNode(renderImage.generatingNode()));

    if (const ImageResource* resource = renderImage.cachedImage())
        data->setString("url", resource->url().string());

    return TracedValue::fromJSONValue(data);
}

static size_t usedHeapSize()
{
    HeapInfo info;
    ScriptGCEvent::getHeapSize(info);
    return info.usedJSHeapSize;
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorUpdateCountersEvent::data()
{
    RefPtr<JSONObject> data = JSONObject::create();
    if (isMainThread()) {
        data->setNumber("documents", InspectorCounters::counterValue(InspectorCounters::DocumentCounter));
        data->setNumber("nodes", InspectorCounters::counterValue(InspectorCounters::NodeCounter));
        data->setNumber("jsEventListeners", InspectorCounters::counterValue(InspectorCounters::JSEventListenerCounter));
    }
    data->setNumber("jsHeapSizeUsed", static_cast<double>(usedHeapSize()));
    return TracedValue::fromJSONValue(data);
}

PassRefPtr<TraceEvent::ConvertableToTraceFormat> InspectorCallStackEvent::currentCallStack()
{
    return adoptRef(new JSCallStack(createScriptCallStack(ScriptCallStack::maxCallStackSizeToCapture, true)));
}

}
