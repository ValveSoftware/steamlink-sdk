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

#ifndef TimelineRecordFactory_h
#define TimelineRecordFactory_h

#include "core/InspectorTypeBuilder.h"
#include "platform/JSONValues.h"
#include "platform/weborigin/KURL.h"
#include "wtf/Forward.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class Event;
class FloatQuad;
class InspectorFrontend;
class IntRect;
class ResourceRequest;
class ResourceResponse;

class TimelineRecordFactory {
public:
    static PassRefPtr<TypeBuilder::Timeline::TimelineEvent> createGenericRecord(double startTime, int maxCallStackDepth, const String& type, PassRefPtr<JSONObject> data);
    static PassRefPtr<TypeBuilder::Timeline::TimelineEvent> createBackgroundRecord(double startTime, const String& thread, const String& type, PassRefPtr<JSONObject> data);

    static PassRefPtr<JSONObject> createGCEventData(size_t usedHeapSizeDelta);
    static PassRefPtr<JSONObject> createFunctionCallData(int scriptId, const String& scriptName, int scriptLine);
    static PassRefPtr<JSONObject> createEventDispatchData(const Event&);
    static PassRefPtr<JSONObject> createGenericTimerData(int timerId);
    static PassRefPtr<JSONObject> createTimerInstallData(int timerId, int timeout, bool singleShot);
    static PassRefPtr<JSONObject> createXHRReadyStateChangeData(const String& url, int readyState);
    static PassRefPtr<JSONObject> createXHRLoadData(const String& url);
    static PassRefPtr<JSONObject> createEvaluateScriptData(const String&, double lineNumber);
    static PassRefPtr<JSONObject> createConsoleTimeData(const String&);
    static PassRefPtr<JSONObject> createTimeStampData(const String&);
    static PassRefPtr<JSONObject> createResourceSendRequestData(const String& requestId, const ResourceRequest&);
    static PassRefPtr<JSONObject> createResourceReceiveResponseData(const String& requestId, const ResourceResponse&);
    static PassRefPtr<JSONObject> createReceiveResourceData(const String& requestId, int length);
    static PassRefPtr<JSONObject> createResourceFinishData(const String& requestId, bool didFail, double finishTime);
    static PassRefPtr<JSONObject> createLayoutData(unsigned dirtyObjects, unsigned totalObjects, bool partialLayout);
    static PassRefPtr<JSONObject> createDecodeImageData(const String& imageType);
    static PassRefPtr<JSONObject> createResizeImageData(bool shouldCache);
    static PassRefPtr<JSONObject> createMarkData(bool isMainFrame);
    static PassRefPtr<JSONObject> createParseHTMLData(unsigned startLine);
    static PassRefPtr<JSONObject> createAnimationFrameData(int callbackId);
    static PassRefPtr<JSONObject> createNodeData(long long nodeId);
    static PassRefPtr<JSONObject> createLayerData(long long layerRootNodeId);
    static PassRefPtr<JSONObject> createFrameData(int frameId);
    static PassRefPtr<JSONObject> createGPUTaskData(bool foreign);

    static void setNodeData(JSONObject* data, long long nodeId);
    static void setLayerData(JSONObject* data, long long layerRootNodeId);
    static void setLayerTreeData(JSONObject* data, PassRefPtr<JSONValue> layerTree);
    static void setPaintData(JSONObject* data, const FloatQuad&, long long layerRootNodeId, int graphicsLayerId);
    static void setLayoutRoot(JSONObject* data, const FloatQuad&, long long rootNodeId);
    static void setStyleRecalcDetails(JSONObject* data, unsigned elementCount);
    static void setImageDetails(JSONObject* data, long long imageElementId, const String& url);

    static inline PassRefPtr<JSONObject> createWebSocketCreateData(unsigned long identifier, const KURL& url, const String& protocol)
    {
        RefPtr<JSONObject> data = JSONObject::create();
        data->setNumber("identifier", identifier);
        data->setString("url", url.string());
        if (!protocol.isNull())
            data->setString("webSocketProtocol", protocol);
        return data.release();
    }

    static inline PassRefPtr<JSONObject> createGenericWebSocketData(unsigned long identifier)
    {
        RefPtr<JSONObject> data = JSONObject::create();
        data->setNumber("identifier", identifier);
        return data.release();
    }

    static PassRefPtr<JSONObject> createEmbedderCallbackData(const String& callbackName);

    static String type(TypeBuilder::Timeline::TimelineEvent*);
private:
    TimelineRecordFactory() { }
};

} // namespace WebCore

#endif // !defined(TimelineRecordFactory_h)
