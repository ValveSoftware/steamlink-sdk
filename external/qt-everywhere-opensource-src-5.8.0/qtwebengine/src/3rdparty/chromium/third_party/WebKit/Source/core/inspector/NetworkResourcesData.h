/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. AND ITS CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE INC.
 * OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef NetworkResourcesData_h
#define NetworkResourcesData_h

#include "core/dom/ContextLifecycleObserver.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "core/inspector/InspectorPageAgent.h"
#include "platform/blob/BlobData.h"
#include "platform/network/HTTPHeaderMap.h"
#include "platform/weborigin/KURL.h"
#include "wtf/Deque.h"
#include "wtf/HashMap.h"
#include "wtf/text/WTFString.h"


namespace blink {

class EncodedFormData;
class ExecutionContext;
class Resource;
class ResourceResponse;
class SharedBuffer;
class TextResourceDecoder;

class XHRReplayData final
    : public GarbageCollectedFinalized<XHRReplayData>
    , public ContextLifecycleObserver {
    USING_GARBAGE_COLLECTED_MIXIN(XHRReplayData);
public:
    static XHRReplayData* create(ExecutionContext*, const AtomicString& method, const KURL&, bool async, PassRefPtr<EncodedFormData>, bool includeCredentials);

    void addHeader(const AtomicString& key, const AtomicString& value);
    const AtomicString& method() const { return m_method; }
    const KURL& url() const { return m_url; }
    bool async() const { return m_async; }
    PassRefPtr<EncodedFormData> formData() const { return m_formData; }
    const HTTPHeaderMap& headers() const { return m_headers; }
    bool includeCredentials() const { return m_includeCredentials; }

    DECLARE_VIRTUAL_TRACE();

private:
    XHRReplayData(ExecutionContext*, const AtomicString& method, const KURL&, bool async, PassRefPtr<EncodedFormData>, bool includeCredentials);

    AtomicString m_method;
    KURL m_url;
    bool m_async;
    RefPtr<EncodedFormData> m_formData;
    HTTPHeaderMap m_headers;
    bool m_includeCredentials;
};

class NetworkResourcesData final : public GarbageCollectedFinalized<NetworkResourcesData> {
public:
    class ResourceData final : public GarbageCollectedFinalized<ResourceData> {
        friend class NetworkResourcesData;
    public:
        ResourceData(NetworkResourcesData*, const String& requestId, const String& loaderId, const KURL&);

        String requestId() const { return m_requestId; }
        String loaderId() const { return m_loaderId; }

        String frameId() const { return m_frameId; }
        void setFrameId(const String& frameId) { m_frameId = frameId; }

        KURL requestedURL() const { return m_requestedURL; }

        bool hasContent() const { return !m_content.isNull(); }
        String content() const { return m_content; }
        void setContent(const String&, bool base64Encoded);

        bool base64Encoded() const { return m_base64Encoded; }

        unsigned removeContent();
        bool isContentEvicted() const { return m_isContentEvicted; }
        unsigned evictContent();

        InspectorPageAgent::ResourceType type() const { return m_type; }
        void setType(InspectorPageAgent::ResourceType type) { m_type = type; }

        int httpStatusCode() const { return m_httpStatusCode; }
        void setHTTPStatusCode(int httpStatusCode) { m_httpStatusCode = httpStatusCode; }

        String mimeType() const { return m_mimeType; }
        void setMimeType(const String& mimeType) { m_mimeType = mimeType; }

        String textEncodingName() const { return m_textEncodingName; }
        void setTextEncodingName(const String& textEncodingName) { m_textEncodingName = textEncodingName; }

        PassRefPtr<SharedBuffer> buffer() const { return m_buffer; }
        void setBuffer(PassRefPtr<SharedBuffer> buffer) { m_buffer = buffer; }

        Resource* cachedResource() const { return m_cachedResource.get(); }
        void setResource(Resource*);

        XHRReplayData* xhrReplayData() const { return m_xhrReplayData.get(); }
        void setXHRReplayData(XHRReplayData* xhrReplayData) { m_xhrReplayData = xhrReplayData; }

        BlobDataHandle* downloadedFileBlob() const { return m_downloadedFileBlob.get(); }
        void setDownloadedFileBlob(PassRefPtr<BlobDataHandle> blob) { m_downloadedFileBlob = blob; }

        DECLARE_TRACE();
    private:
        bool hasData() const { return m_dataBuffer.get(); }
        size_t dataLength() const;
        void appendData(const char* data, size_t dataLength);
        size_t decodeDataToContent();
        void clearWeakMembers(Visitor*);

        Member<NetworkResourcesData> m_networkResourcesData;
        String m_requestId;
        String m_loaderId;
        String m_frameId;
        KURL m_requestedURL;
        String m_content;
        Member<XHRReplayData> m_xhrReplayData;
        bool m_base64Encoded;
        RefPtr<SharedBuffer> m_dataBuffer;
        bool m_isContentEvicted;
        InspectorPageAgent::ResourceType m_type;
        int m_httpStatusCode;

        String m_mimeType;
        String m_textEncodingName;

        RefPtr<SharedBuffer> m_buffer;
        WeakMember<Resource> m_cachedResource;
        RefPtr<BlobDataHandle> m_downloadedFileBlob;
    };

    static NetworkResourcesData* create(size_t totalBufferSize, size_t resourceBufferSize)
    {
        return new NetworkResourcesData(totalBufferSize, resourceBufferSize);
    }
    ~NetworkResourcesData();

    void resourceCreated(const String& requestId, const String& loaderId, const KURL&);
    void responseReceived(const String& requestId, const String& frameId, const ResourceResponse&);
    void setResourceType(const String& requestId, InspectorPageAgent::ResourceType);
    InspectorPageAgent::ResourceType resourceType(const String& requestId);
    void setResourceContent(const String& requestId, const String& content, bool base64Encoded = false);
    void maybeAddResourceData(const String& requestId, const char* data, size_t dataLength);
    void maybeDecodeDataToContent(const String& requestId);
    void addResource(const String& requestId, Resource*);
    ResourceData const* data(const String& requestId);
    void clear(const String& preservedLoaderId = String());

    void setResourcesDataSizeLimits(size_t maximumResourcesContentSize, size_t maximumSingleResourceContentSize);
    void setXHRReplayData(const String& requestId, XHRReplayData*);
    XHRReplayData* xhrReplayData(const String& requestId);
    HeapVector<Member<ResourceData>> resources();

    DECLARE_TRACE();
private:
    NetworkResourcesData(size_t totalBufferSize, size_t resourceBufferSize);

    ResourceData* resourceDataForRequestId(const String& requestId);
    void ensureNoDataForRequestId(const String& requestId);
    bool ensureFreeSpace(size_t);

    Deque<String> m_requestIdsDeque;

    typedef HashMap<String, String> ReusedRequestIds;
    ReusedRequestIds m_reusedXHRReplayDataRequestIds;
    typedef HeapHashMap<String, Member<ResourceData>> ResourceDataMap;
    ResourceDataMap m_requestIdToResourceDataMap;
    size_t m_contentSize;
    size_t m_maximumResourcesContentSize;
    size_t m_maximumSingleResourceContentSize;
};

} // namespace blink


#endif // !defined(NetworkResourcesData_h)
