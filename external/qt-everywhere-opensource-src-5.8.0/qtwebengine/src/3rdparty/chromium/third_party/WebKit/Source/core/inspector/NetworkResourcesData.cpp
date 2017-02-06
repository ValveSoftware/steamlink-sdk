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

#include "core/inspector/NetworkResourcesData.h"

#include "core/dom/DOMImplementation.h"
#include "core/fetch/Resource.h"
#include "platform/SharedBuffer.h"
#include "platform/network/ResourceResponse.h"
#include <memory>

namespace blink {

static bool isErrorStatusCode(int statusCode)
{
    return statusCode >= 400;
}

XHRReplayData* XHRReplayData::create(ExecutionContext* executionContext, const AtomicString& method, const KURL& url, bool async, PassRefPtr<EncodedFormData> formData, bool includeCredentials)
{
    return new XHRReplayData(executionContext, method, url, async, formData, includeCredentials);
}

void XHRReplayData::addHeader(const AtomicString& key, const AtomicString& value)
{
    m_headers.set(key, value);
}

XHRReplayData::XHRReplayData(ExecutionContext* executionContext, const AtomicString& method, const KURL& url, bool async, PassRefPtr<EncodedFormData> formData, bool includeCredentials)
    : ContextLifecycleObserver(executionContext)
    , m_method(method)
    , m_url(url)
    , m_async(async)
    , m_formData(formData)
    , m_includeCredentials(includeCredentials)
{
}

DEFINE_TRACE(XHRReplayData)
{
    ContextLifecycleObserver::trace(visitor);
}

// ResourceData
NetworkResourcesData::ResourceData::ResourceData(NetworkResourcesData* networkResourcesData, const String& requestId, const String& loaderId, const KURL& requestedURL)
    : m_networkResourcesData(networkResourcesData)
    , m_requestId(requestId)
    , m_loaderId(loaderId)
    , m_requestedURL(requestedURL)
    , m_base64Encoded(false)
    , m_isContentEvicted(false)
    , m_type(InspectorPageAgent::OtherResource)
    , m_httpStatusCode(0)
    , m_cachedResource(nullptr)
{
}

DEFINE_TRACE(NetworkResourcesData::ResourceData)
{
    visitor->trace(m_networkResourcesData);
    visitor->trace(m_xhrReplayData);
    visitor->template registerWeakMembers<NetworkResourcesData::ResourceData, &NetworkResourcesData::ResourceData::clearWeakMembers>(this);
}

void NetworkResourcesData::ResourceData::setContent(const String& content, bool base64Encoded)
{
    ASSERT(!hasData());
    ASSERT(!hasContent());
    m_content = content;
    m_base64Encoded = base64Encoded;
}

static size_t contentSizeInBytes(const String& content)
{
    return content.isNull() ? 0 : content.impl()->sizeInBytes();
}

unsigned NetworkResourcesData::ResourceData::removeContent()
{
    unsigned result = 0;
    if (hasData()) {
        ASSERT(!hasContent());
        result = m_dataBuffer->size();
        m_dataBuffer = nullptr;
    }

    if (hasContent()) {
        ASSERT(!hasData());
        result = contentSizeInBytes(m_content);
        m_content = String();
    }
    return result;
}

unsigned NetworkResourcesData::ResourceData::evictContent()
{
    m_isContentEvicted = true;
    return removeContent();
}

void NetworkResourcesData::ResourceData::setResource(Resource* cachedResource)
{
    m_cachedResource = cachedResource;
}

void NetworkResourcesData::ResourceData::clearWeakMembers(Visitor* visitor)
{
    if (!m_cachedResource || ThreadHeap::isHeapObjectAlive(m_cachedResource))
        return;

    // Mark loaded resources or resources without the buffer as loaded.
    if (m_cachedResource->isLoaded() || !m_cachedResource->resourceBuffer()) {
        if (!isErrorStatusCode(m_cachedResource->response().httpStatusCode())) {
            String content;
            bool base64Encoded;
            if (InspectorPageAgent::cachedResourceContent(m_cachedResource, &content, &base64Encoded))
                m_networkResourcesData->setResourceContent(requestId(), content, base64Encoded);
        }
    } else {
        // We could be evicting resource being loaded, save the loaded part, the rest will be appended.
        m_networkResourcesData->maybeAddResourceData(requestId(), m_cachedResource->resourceBuffer()->data(), m_cachedResource->resourceBuffer()->size());
    }
    m_cachedResource = nullptr;
}

size_t NetworkResourcesData::ResourceData::dataLength() const
{
    return m_dataBuffer ? m_dataBuffer->size() : 0;
}

void NetworkResourcesData::ResourceData::appendData(const char* data, size_t dataLength)
{
    ASSERT(!hasContent());
    if (!m_dataBuffer)
        m_dataBuffer = SharedBuffer::create(data, dataLength);
    else
        m_dataBuffer->append(data, dataLength);
}

size_t NetworkResourcesData::ResourceData::decodeDataToContent()
{
    DCHECK(!hasContent());
    DCHECK(hasData());
    size_t dataLength = m_dataBuffer->size();
    bool success = InspectorPageAgent::sharedBufferContent(m_dataBuffer, m_mimeType, m_textEncodingName, &m_content, &m_base64Encoded);
    DCHECK(success);
    m_dataBuffer = nullptr;
    return contentSizeInBytes(m_content) - dataLength;
}

// NetworkResourcesData
NetworkResourcesData::NetworkResourcesData(size_t totalBufferSize, size_t resourceBufferSize)
    : m_contentSize(0)
    , m_maximumResourcesContentSize(totalBufferSize)
    , m_maximumSingleResourceContentSize(resourceBufferSize)
{
}

NetworkResourcesData::~NetworkResourcesData()
{
}

DEFINE_TRACE(NetworkResourcesData)
{
    visitor->trace(m_requestIdToResourceDataMap);
}

void NetworkResourcesData::resourceCreated(const String& requestId, const String& loaderId, const KURL& requestedURL)
{
    ensureNoDataForRequestId(requestId);
    m_requestIdToResourceDataMap.set(requestId, new ResourceData(this, requestId, loaderId, requestedURL));
}

void NetworkResourcesData::responseReceived(const String& requestId, const String& frameId, const ResourceResponse& response)
{
    ResourceData* resourceData = resourceDataForRequestId(requestId);
    if (!resourceData)
        return;
    resourceData->setFrameId(frameId);
    resourceData->setMimeType(response.mimeType());
    resourceData->setTextEncodingName(response.textEncodingName());
    resourceData->setHTTPStatusCode(response.httpStatusCode());

    String filePath = response.downloadedFilePath();
    if (!filePath.isEmpty()) {
        std::unique_ptr<BlobData> blobData = BlobData::create();
        blobData->appendFile(filePath);
        AtomicString mimeType;
        if (response.isHTTP())
            mimeType = extractMIMETypeFromMediaType(response.httpHeaderField(HTTPNames::Content_Type));
        if (mimeType.isEmpty())
            mimeType = response.mimeType();
        if (mimeType.isEmpty())
            mimeType = AtomicString("text/plain");
        blobData->setContentType(mimeType);
        resourceData->setDownloadedFileBlob(BlobDataHandle::create(std::move(blobData), -1));
    }
}

void NetworkResourcesData::setResourceType(const String& requestId, InspectorPageAgent::ResourceType type)
{
    ResourceData* resourceData = resourceDataForRequestId(requestId);
    if (!resourceData)
        return;
    resourceData->setType(type);
}

InspectorPageAgent::ResourceType NetworkResourcesData::resourceType(const String& requestId)
{
    ResourceData* resourceData = resourceDataForRequestId(requestId);
    if (!resourceData)
        return InspectorPageAgent::OtherResource;
    return resourceData->type();
}

void NetworkResourcesData::setResourceContent(const String& requestId, const String& content, bool base64Encoded)
{
    ResourceData* resourceData = resourceDataForRequestId(requestId);
    if (!resourceData)
        return;
    size_t dataLength = contentSizeInBytes(content);
    if (dataLength > m_maximumSingleResourceContentSize)
        return;
    if (resourceData->isContentEvicted())
        return;
    if (ensureFreeSpace(dataLength) && !resourceData->isContentEvicted()) {
        // We can not be sure that we didn't try to save this request data while it was loading, so remove it, if any.
        if (resourceData->hasContent())
            m_contentSize -= resourceData->removeContent();
        m_requestIdsDeque.append(requestId);
        resourceData->setContent(content, base64Encoded);
        m_contentSize += dataLength;
    }
}

void NetworkResourcesData::maybeAddResourceData(const String& requestId, const char* data, size_t dataLength)
{
    ResourceData* resourceData = resourceDataForRequestId(requestId);
    if (!resourceData)
        return;
    if (resourceData->dataLength() + dataLength > m_maximumSingleResourceContentSize)
        m_contentSize -= resourceData->evictContent();
    if (resourceData->isContentEvicted())
        return;
    if (ensureFreeSpace(dataLength) && !resourceData->isContentEvicted()) {
        m_requestIdsDeque.append(requestId);
        resourceData->appendData(data, dataLength);
        m_contentSize += dataLength;
    }
}

void NetworkResourcesData::maybeDecodeDataToContent(const String& requestId)
{
    ResourceData* resourceData = resourceDataForRequestId(requestId);
    if (!resourceData)
        return;
    if (!resourceData->hasData())
        return;
    m_contentSize += resourceData->decodeDataToContent();
    size_t dataLength = contentSizeInBytes(resourceData->content());
    if (dataLength > m_maximumSingleResourceContentSize)
        m_contentSize -= resourceData->evictContent();
}

void NetworkResourcesData::addResource(const String& requestId, Resource* cachedResource)
{
    ResourceData* resourceData = resourceDataForRequestId(requestId);
    if (!resourceData)
        return;
    resourceData->setResource(cachedResource);
}

NetworkResourcesData::ResourceData const* NetworkResourcesData::data(const String& requestId)
{
    return resourceDataForRequestId(requestId);
}

XHRReplayData* NetworkResourcesData::xhrReplayData(const String& requestId)
{
    if (m_reusedXHRReplayDataRequestIds.contains(requestId))
        return xhrReplayData(m_reusedXHRReplayDataRequestIds.get(requestId));

    ResourceData* resourceData = resourceDataForRequestId(requestId);
    if (!resourceData)
        return 0;
    return resourceData->xhrReplayData();
}

void NetworkResourcesData::setXHRReplayData(const String& requestId, XHRReplayData* xhrReplayData)
{
    ResourceData* resourceData = resourceDataForRequestId(requestId);
    if (!resourceData) {
        Vector<String> result;
        for (auto& request : m_reusedXHRReplayDataRequestIds) {
            if (request.value == requestId)
                setXHRReplayData(request.key, xhrReplayData);
        }
        return;
    }

    resourceData->setXHRReplayData(xhrReplayData);
}

HeapVector<Member<NetworkResourcesData::ResourceData>> NetworkResourcesData::resources()
{
    HeapVector<Member<ResourceData>> result;
    for (auto& request : m_requestIdToResourceDataMap)
        result.append(request.value);
    return result;
}

void NetworkResourcesData::clear(const String& preservedLoaderId)
{
    if (!m_requestIdToResourceDataMap.size())
        return;
    m_requestIdsDeque.clear();
    m_contentSize = 0;

    ResourceDataMap preservedMap;

    for (auto& resource : m_requestIdToResourceDataMap) {
        ResourceData* resourceData = resource.value;
        if (!preservedLoaderId.isNull() && resourceData->loaderId() == preservedLoaderId)
            preservedMap.set(resource.key, resource.value);
    }
    m_requestIdToResourceDataMap.swap(preservedMap);

    m_reusedXHRReplayDataRequestIds.clear();
}

void NetworkResourcesData::setResourcesDataSizeLimits(size_t resourcesContentSize, size_t singleResourceContentSize)
{
    clear();
    m_maximumResourcesContentSize = resourcesContentSize;
    m_maximumSingleResourceContentSize = singleResourceContentSize;
}

NetworkResourcesData::ResourceData* NetworkResourcesData::resourceDataForRequestId(const String& requestId)
{
    if (requestId.isNull())
        return 0;
    return m_requestIdToResourceDataMap.get(requestId);
}

void NetworkResourcesData::ensureNoDataForRequestId(const String& requestId)
{
    ResourceData* resourceData = resourceDataForRequestId(requestId);
    if (!resourceData)
        return;
    if (resourceData->hasContent() || resourceData->hasData())
        m_contentSize -= resourceData->evictContent();
    m_requestIdToResourceDataMap.remove(requestId);
}

bool NetworkResourcesData::ensureFreeSpace(size_t size)
{
    if (size > m_maximumResourcesContentSize)
        return false;

    while (size > m_maximumResourcesContentSize - m_contentSize) {
        String requestId = m_requestIdsDeque.takeFirst();
        ResourceData* resourceData = resourceDataForRequestId(requestId);
        if (resourceData)
            m_contentSize -= resourceData->evictContent();
    }
    return true;
}

} // namespace blink
