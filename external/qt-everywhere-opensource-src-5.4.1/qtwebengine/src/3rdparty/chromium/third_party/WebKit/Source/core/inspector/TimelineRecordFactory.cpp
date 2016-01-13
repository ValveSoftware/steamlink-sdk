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

#include "config.h"
#include "core/inspector/TimelineRecordFactory.h"

#include "bindings/v8/ScriptCallStackFactory.h"
#include "core/events/Event.h"
#include "core/inspector/ScriptCallStack.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"
#include "wtf/CurrentTime.h"

namespace WebCore {

using TypeBuilder::Timeline::TimelineEvent;

PassRefPtr<TimelineEvent> TimelineRecordFactory::createGenericRecord(double startTime, int maxCallStackDepth, const String& type, PassRefPtr<JSONObject> data)
{
    ASSERT(data.get());
    RefPtr<TimelineEvent> record = TimelineEvent::create()
        .setType(type)
        .setData(data)
        .setStartTime(startTime);
    if (maxCallStackDepth) {
        RefPtrWillBeRawPtr<ScriptCallStack> stackTrace = createScriptCallStack(maxCallStackDepth, true);
        if (stackTrace && stackTrace->size())
            record->setStackTrace(stackTrace->buildInspectorArray());
    }
    return record.release();
}

PassRefPtr<TimelineEvent> TimelineRecordFactory::createBackgroundRecord(double startTime, const String& threadName, const String& type, PassRefPtr<JSONObject> data)
{
    ASSERT(data.get());
    RefPtr<TimelineEvent> record = TimelineEvent::create()
        .setType(type)
        .setData(data)
        .setStartTime(startTime);
    record->setThread(threadName);
    return record.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createGCEventData(size_t usedHeapSizeDelta)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setNumber("usedHeapSizeDelta", usedHeapSizeDelta);
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createFunctionCallData(int scriptId, const String& scriptName, int scriptLine)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("scriptId", String::number(scriptId));
    data->setString("scriptName", scriptName);
    data->setNumber("scriptLine", scriptLine);
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createEventDispatchData(const Event& event)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("type", event.type().string());
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createGenericTimerData(int timerId)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setNumber("timerId", timerId);
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createTimerInstallData(int timerId, int timeout, bool singleShot)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setNumber("timerId", timerId);
    data->setNumber("timeout", timeout);
    data->setBoolean("singleShot", singleShot);
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createXHRReadyStateChangeData(const String& url, int readyState)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("url", url);
    data->setNumber("readyState", readyState);
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createXHRLoadData(const String& url)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("url", url);
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createEvaluateScriptData(const String& url, double lineNumber)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("url", url);
    data->setNumber("lineNumber", lineNumber);
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createConsoleTimeData(const String& message)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("message", message);
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createTimeStampData(const String& message)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("message", message);
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createResourceSendRequestData(const String& requestId, const ResourceRequest& request)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("requestId", requestId);
    data->setString("url", request.url().string());
    data->setString("requestMethod", request.httpMethod());
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createResourceReceiveResponseData(const String& requestId, const ResourceResponse& response)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("requestId", requestId);
    data->setNumber("statusCode", response.httpStatusCode());
    data->setString("mimeType", response.mimeType());
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createResourceFinishData(const String& requestId, bool didFail, double finishTime)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("requestId", requestId);
    data->setBoolean("didFail", didFail);
    if (finishTime)
        data->setNumber("networkTime", finishTime);
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createReceiveResourceData(const String& requestId, int length)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("requestId", requestId);
    data->setNumber("encodedDataLength", length);
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createLayoutData(unsigned dirtyObjects, unsigned totalObjects, bool partialLayout)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setNumber("dirtyObjects", dirtyObjects);
    data->setNumber("totalObjects", totalObjects);
    data->setBoolean("partialLayout", partialLayout);
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createDecodeImageData(const String& imageType)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("imageType", imageType);
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createResizeImageData(bool shouldCache)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setBoolean("cached", shouldCache);
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createMarkData(bool isMainFrame)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setBoolean("isMainFrame", isMainFrame);
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createParseHTMLData(unsigned startLine)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setNumber("startLine", startLine);
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createAnimationFrameData(int callbackId)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setNumber("id", callbackId);
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createGPUTaskData(bool foreign)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setBoolean("foreign", foreign);
    return data.release();
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

PassRefPtr<JSONObject> TimelineRecordFactory::createNodeData(long long nodeId)
{
    RefPtr<JSONObject> data = JSONObject::create();
    setNodeData(data.get(), nodeId);
    return data.release();
}

PassRefPtr<JSONObject> TimelineRecordFactory::createLayerData(long long rootNodeId)
{
    return createNodeData(rootNodeId);
}

void TimelineRecordFactory::setLayerTreeData(JSONObject* data, PassRefPtr<JSONValue> layerTree)
{
    data->setValue("layerTree", layerTree);
}

void TimelineRecordFactory::setNodeData(JSONObject* data, long long nodeId)
{
    if (nodeId)
        data->setNumber("backendNodeId", nodeId);
}

void TimelineRecordFactory::setLayerData(JSONObject* data, long long rootNodeId)
{
    setNodeData(data, rootNodeId);
}

void TimelineRecordFactory::setPaintData(JSONObject* data, const FloatQuad& quad, long long layerRootNodeId, int graphicsLayerId)
{
    setLayerData(data, layerRootNodeId);
    data->setArray("clip", createQuad(quad));
    data->setNumber("layerId", graphicsLayerId);
}

PassRefPtr<JSONObject> TimelineRecordFactory::createFrameData(int frameId)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setNumber("id", frameId);
    return data.release();
}

void TimelineRecordFactory::setLayoutRoot(JSONObject* data, const FloatQuad& quad, long long rootNodeId)
{
    data->setArray("root", createQuad(quad));
    if (rootNodeId)
        data->setNumber("backendNodeId", rootNodeId);
}

void TimelineRecordFactory::setStyleRecalcDetails(JSONObject* data, unsigned elementCount)
{
    data->setNumber("elementCount", elementCount);
}

void TimelineRecordFactory::setImageDetails(JSONObject* data, long long imageElementId, const String& url)
{
    if (imageElementId)
        data->setNumber("backendNodeId", imageElementId);
    if (!url.isEmpty())
        data->setString("url", url);
}

PassRefPtr<JSONObject> TimelineRecordFactory::createEmbedderCallbackData(const String& callbackName)
{
    RefPtr<JSONObject> data = JSONObject::create();
    data->setString("callbackName", callbackName);
    return data.release();
}

String TimelineRecordFactory::type(TypeBuilder::Timeline::TimelineEvent* event)
{
    String type;
    event->type(&type);
    return type;
}

} // namespace WebCore

