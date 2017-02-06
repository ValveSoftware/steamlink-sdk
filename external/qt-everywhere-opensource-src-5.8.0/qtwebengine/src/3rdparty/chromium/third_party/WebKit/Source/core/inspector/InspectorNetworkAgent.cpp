/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "core/inspector/InspectorNetworkAgent.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "bindings/core/v8/SourceLocation.h"
#include "core/dom/Document.h"
#include "core/dom/ScriptableDocumentParser.h"
#include "core/fetch/FetchInitiatorInfo.h"
#include "core/fetch/FetchInitiatorTypeNames.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/Resource.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/fetch/ResourceLoader.h"
#include "core/fetch/UniqueIdentifier.h"
#include "core/fileapi/FileReaderLoader.h"
#include "core/fileapi/FileReaderLoaderClient.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/FrameHost.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectedFrames.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/NetworkResourcesData.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/MixedContentChecker.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "core/page/NetworkStateNotifier.h"
#include "core/page/Page.h"
#include "core/xmlhttprequest/XMLHttpRequest.h"
#include "platform/blob/BlobData.h"
#include "platform/inspector_protocol/Values.h"
#include "platform/network/HTTPHeaderMap.h"
#include "platform/network/ResourceError.h"
#include "platform/network/ResourceLoadTiming.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"
#include "platform/network/WebSocketHandshakeRequest.h"
#include "platform/network/WebSocketHandshakeResponse.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebCachePolicy.h"
#include "public/platform/WebMixedContent.h"
#include "public/platform/WebURLRequest.h"
#include "wtf/CurrentTime.h"
#include "wtf/RefPtr.h"
#include "wtf/text/Base64.h"
#include <memory>

namespace blink {

using GetResponseBodyCallback = protocol::Network::Backend::GetResponseBodyCallback;

namespace NetworkAgentState {
static const char networkAgentEnabled[] = "networkAgentEnabled";
static const char extraRequestHeaders[] = "extraRequestHeaders";
static const char cacheDisabled[] = "cacheDisabled";
static const char bypassServiceWorker[] = "bypassServiceWorker";
static const char userAgentOverride[] = "userAgentOverride";
static const char monitoringXHR[] = "monitoringXHR";
static const char blockedURLs[] = "blockedURLs";
static const char totalBufferSize[] = "totalBufferSize";
static const char resourceBufferSize[] = "resourceBufferSize";
}

namespace {
// 100MB
static size_t maximumTotalBufferSize = 100 * 1000 * 1000;

// 10MB
static size_t maximumResourceBufferSize = 10 * 1000 * 1000;

// Pattern may contain stars ('*') which match to any (possibly empty) string.
// Stars implicitly assumed at the begin/end of pattern.
bool matches(const String& url, const String& pattern)
{
    Vector<String> parts;
    pattern.split("*", parts);
    size_t pos = 0;
    for (const String& part : parts) {
        pos = url.find(part, pos);
        if (pos == kNotFound)
            return false;
        pos += part.length();
    }
    return true;
}

static std::unique_ptr<protocol::Network::Headers> buildObjectForHeaders(const HTTPHeaderMap& headers)
{
    std::unique_ptr<protocol::DictionaryValue> headersObject = protocol::DictionaryValue::create();
    for (const auto& header : headers)
        headersObject->setString(header.key.getString(), header.value);
    protocol::ErrorSupport errors;
    return protocol::Network::Headers::parse(headersObject.get(), &errors);
}

class InspectorFileReaderLoaderClient final : public FileReaderLoaderClient {
    WTF_MAKE_NONCOPYABLE(InspectorFileReaderLoaderClient);
public:
    InspectorFileReaderLoaderClient(PassRefPtr<BlobDataHandle> blob, const String& mimeType, const String& textEncodingName, std::unique_ptr<GetResponseBodyCallback> callback)
        : m_blob(blob)
        , m_mimeType(mimeType)
        , m_textEncodingName(textEncodingName)
        , m_callback(std::move(callback))
    {
        m_loader = FileReaderLoader::create(FileReaderLoader::ReadByClient, this);
    }

    ~InspectorFileReaderLoaderClient() override { }

    void start(ExecutionContext* executionContext)
    {
        m_rawData = SharedBuffer::create();
        m_loader->start(executionContext, m_blob);
    }

    virtual void didStartLoading() { }

    virtual void didReceiveDataForClient(const char* data, unsigned dataLength)
    {
        if (!dataLength)
            return;
        m_rawData->append(data, dataLength);
    }

    virtual void didFinishLoading()
    {
        String result;
        bool base64Encoded;
        if (InspectorPageAgent::sharedBufferContent(m_rawData, m_mimeType, m_textEncodingName, &result, &base64Encoded))
            m_callback->sendSuccess(result, base64Encoded);
        else
            m_callback->sendFailure("Couldn't encode data");
        dispose();
    }

    virtual void didFail(FileError::ErrorCode)
    {
        m_callback->sendFailure("Couldn't read BLOB");
        dispose();
    }

private:
    void dispose()
    {
        m_rawData.clear();
        delete this;
    }

    RefPtr<BlobDataHandle> m_blob;
    String m_mimeType;
    String m_textEncodingName;
    std::unique_ptr<GetResponseBodyCallback> m_callback;
    std::unique_ptr<FileReaderLoader> m_loader;
    RefPtr<SharedBuffer> m_rawData;
};

KURL urlWithoutFragment(const KURL& url)
{
    KURL result = url;
    result.removeFragmentIdentifier();
    return result;
}

String mixedContentTypeForContextType(WebMixedContent::ContextType contextType)
{
    switch (contextType) {
    case WebMixedContent::ContextType::NotMixedContent:
        return protocol::Network::Request::MixedContentTypeEnum::None;
    case WebMixedContent::ContextType::Blockable:
        return protocol::Network::Request::MixedContentTypeEnum::Blockable;
    case WebMixedContent::ContextType::OptionallyBlockable:
    case WebMixedContent::ContextType::ShouldBeBlockable:
        return protocol::Network::Request::MixedContentTypeEnum::OptionallyBlockable;
    }

    return protocol::Network::Request::MixedContentTypeEnum::None;
}

String resourcePriorityJSON(ResourceLoadPriority priority)
{
    switch (priority) {
    case ResourceLoadPriorityVeryLow: return protocol::Network::ResourcePriorityEnum::VeryLow;
    case ResourceLoadPriorityLow: return protocol::Network::ResourcePriorityEnum::Low;
    case ResourceLoadPriorityMedium: return protocol::Network::ResourcePriorityEnum::Medium;
    case ResourceLoadPriorityHigh: return protocol::Network::ResourcePriorityEnum::High;
    case ResourceLoadPriorityVeryHigh: return protocol::Network::ResourcePriorityEnum::VeryHigh;
    case ResourceLoadPriorityUnresolved: break;
    }
    NOTREACHED();
    return protocol::Network::ResourcePriorityEnum::Medium;
}

String buildBlockedReason(ResourceRequestBlockedReason reason)
{
    switch (reason) {
    case ResourceRequestBlockedReasonCSP:
        return protocol::Network::BlockedReasonEnum::Csp;
    case ResourceRequestBlockedReasonMixedContent:
        return protocol::Network::BlockedReasonEnum::MixedContent;
    case ResourceRequestBlockedReasonOrigin:
        return protocol::Network::BlockedReasonEnum::Origin;
    case ResourceRequestBlockedReasonInspector:
        return protocol::Network::BlockedReasonEnum::Inspector;
    case ResourceRequestBlockedReasonSubresourceFilter:
        return protocol::Network::BlockedReasonEnum::SubresourceFilter;
    case ResourceRequestBlockedReasonOther:
        return protocol::Network::BlockedReasonEnum::Other;
    case ResourceRequestBlockedReasonNone:
    default:
        NOTREACHED();
        return protocol::Network::BlockedReasonEnum::Other;
    }
}

WebConnectionType toWebConnectionType(ErrorString* errorString, const String& connectionType)
{
    if (connectionType == protocol::Network::ConnectionTypeEnum::None)
        return WebConnectionTypeNone;
    if (connectionType == protocol::Network::ConnectionTypeEnum::Cellular2g)
        return WebConnectionTypeCellular2G;
    if (connectionType == protocol::Network::ConnectionTypeEnum::Cellular3g)
        return WebConnectionTypeCellular3G;
    if (connectionType == protocol::Network::ConnectionTypeEnum::Cellular4g)
        return WebConnectionTypeCellular4G;
    if (connectionType == protocol::Network::ConnectionTypeEnum::Bluetooth)
        return WebConnectionTypeBluetooth;
    if (connectionType == protocol::Network::ConnectionTypeEnum::Ethernet)
        return WebConnectionTypeEthernet;
    if (connectionType == protocol::Network::ConnectionTypeEnum::Wifi)
        return WebConnectionTypeWifi;
    if (connectionType == protocol::Network::ConnectionTypeEnum::Wimax)
        return WebConnectionTypeWimax;
    if (connectionType == protocol::Network::ConnectionTypeEnum::Other)
        return WebConnectionTypeOther;
    *errorString = "Unknown connection type";
    return WebConnectionTypeUnknown;
}

} // namespace

void InspectorNetworkAgent::restore()
{
    if (m_state->booleanProperty(NetworkAgentState::networkAgentEnabled, false)) {
        enable(
            m_state->numberProperty(NetworkAgentState::totalBufferSize, maximumTotalBufferSize),
            m_state->numberProperty(NetworkAgentState::resourceBufferSize, maximumResourceBufferSize));
    }
}

static std::unique_ptr<protocol::Network::ResourceTiming> buildObjectForTiming(const ResourceLoadTiming& timing)
{
    return protocol::Network::ResourceTiming::create()
        .setRequestTime(timing.requestTime())
        .setProxyStart(timing.calculateMillisecondDelta(timing.proxyStart()))
        .setProxyEnd(timing.calculateMillisecondDelta(timing.proxyEnd()))
        .setDnsStart(timing.calculateMillisecondDelta(timing.dnsStart()))
        .setDnsEnd(timing.calculateMillisecondDelta(timing.dnsEnd()))
        .setConnectStart(timing.calculateMillisecondDelta(timing.connectStart()))
        .setConnectEnd(timing.calculateMillisecondDelta(timing.connectEnd()))
        .setSslStart(timing.calculateMillisecondDelta(timing.sslStart()))
        .setSslEnd(timing.calculateMillisecondDelta(timing.sslEnd()))
        .setWorkerStart(timing.calculateMillisecondDelta(timing.workerStart()))
        .setWorkerReady(timing.calculateMillisecondDelta(timing.workerReady()))
        .setSendStart(timing.calculateMillisecondDelta(timing.sendStart()))
        .setSendEnd(timing.calculateMillisecondDelta(timing.sendEnd()))
        .setReceiveHeadersEnd(timing.calculateMillisecondDelta(timing.receiveHeadersEnd()))
        .setPushStart(timing.pushStart())
        .setPushEnd(timing.pushEnd())
        .build();
}

static std::unique_ptr<protocol::Network::Request> buildObjectForResourceRequest(const ResourceRequest& request)
{
    std::unique_ptr<protocol::Network::Request> requestObject = protocol::Network::Request::create()
        .setUrl(urlWithoutFragment(request.url()).getString())
        .setMethod(request.httpMethod())
        .setHeaders(buildObjectForHeaders(request.httpHeaderFields()))
        .setInitialPriority(resourcePriorityJSON(request.priority())).build();
    if (request.httpBody() && !request.httpBody()->isEmpty()) {
        Vector<char> bytes;
        request.httpBody()->flatten(bytes);
        requestObject->setPostData(String::fromUTF8WithLatin1Fallback(bytes.data(), bytes.size()));
    }
    return requestObject;
}

static std::unique_ptr<protocol::Network::Response> buildObjectForResourceResponse(const ResourceResponse& response, Resource* cachedResource = nullptr, bool* isEmpty = nullptr)
{
    if (response.isNull())
        return nullptr;

    double status;
    String statusText;
    if (response.resourceLoadInfo() && response.resourceLoadInfo()->httpStatusCode) {
        status = response.resourceLoadInfo()->httpStatusCode;
        statusText = response.resourceLoadInfo()->httpStatusText;
    } else {
        status = response.httpStatusCode();
        statusText = response.httpStatusText();
    }
    HTTPHeaderMap headersMap;
    if (response.resourceLoadInfo() && response.resourceLoadInfo()->responseHeaders.size())
        headersMap = response.resourceLoadInfo()->responseHeaders;
    else
        headersMap = response.httpHeaderFields();

    int64_t encodedDataLength = response.resourceLoadInfo() ? response.resourceLoadInfo()->encodedDataLength : -1;

    String securityState = protocol::Security::SecurityStateEnum::Unknown;
    switch (response.getSecurityStyle()) {
    case ResourceResponse::SecurityStyleUnknown:
        securityState = protocol::Security::SecurityStateEnum::Unknown;
        break;
    case ResourceResponse::SecurityStyleUnauthenticated:
        securityState = protocol::Security::SecurityStateEnum::Neutral;
        break;
    case ResourceResponse::SecurityStyleAuthenticationBroken:
        securityState = protocol::Security::SecurityStateEnum::Insecure;
        break;
    case ResourceResponse::SecurityStyleWarning:
        securityState = protocol::Security::SecurityStateEnum::Warning;
        break;
    case ResourceResponse::SecurityStyleAuthenticated:
        securityState = protocol::Security::SecurityStateEnum::Secure;
        break;
    }

    // Use mime type from cached resource in case the one in response is empty.
    String mimeType = response.mimeType();
    if (mimeType.isEmpty() && cachedResource)
        mimeType = cachedResource->response().mimeType();

    if (isEmpty)
        *isEmpty = !status && mimeType.isEmpty() && !headersMap.size();

    std::unique_ptr<protocol::Network::Response> responseObject = protocol::Network::Response::create()
        .setUrl(urlWithoutFragment(response.url()).getString())
        .setStatus(status)
        .setStatusText(statusText)
        .setHeaders(buildObjectForHeaders(headersMap))
        .setMimeType(mimeType)
        .setConnectionReused(response.connectionReused())
        .setConnectionId(response.connectionID())
        .setEncodedDataLength(encodedDataLength)
        .setSecurityState(securityState).build();

    responseObject->setFromDiskCache(response.wasCached());
    responseObject->setFromServiceWorker(response.wasFetchedViaServiceWorker());
    if (response.resourceLoadTiming())
        responseObject->setTiming(buildObjectForTiming(*response.resourceLoadTiming()));

    if (response.resourceLoadInfo()) {
        if (!response.resourceLoadInfo()->responseHeadersText.isEmpty())
            responseObject->setHeadersText(response.resourceLoadInfo()->responseHeadersText);
        if (response.resourceLoadInfo()->requestHeaders.size())
            responseObject->setRequestHeaders(buildObjectForHeaders(response.resourceLoadInfo()->requestHeaders));
        if (!response.resourceLoadInfo()->requestHeadersText.isEmpty())
            responseObject->setRequestHeadersText(response.resourceLoadInfo()->requestHeadersText);
    }

    String remoteIPAddress = response.remoteIPAddress();
    if (!remoteIPAddress.isEmpty()) {
        responseObject->setRemoteIPAddress(remoteIPAddress);
        responseObject->setRemotePort(response.remotePort());
    }

    String protocol;
    if (response.resourceLoadInfo())
        protocol = response.resourceLoadInfo()->npnNegotiatedProtocol;
    if (protocol.isEmpty() || protocol == "unknown") {
        if (response.wasFetchedViaSPDY()) {
            protocol = "spdy";
        } else if (response.isHTTP()) {
            protocol = "http";
            if (response.httpVersion() == ResourceResponse::HTTPVersion::HTTPVersion_0_9)
                protocol = "http/0.9";
            else if (response.httpVersion() == ResourceResponse::HTTPVersion::HTTPVersion_1_0)
                protocol = "http/1.0";
            else if (response.httpVersion() == ResourceResponse::HTTPVersion::HTTPVersion_1_1)
                protocol = "http/1.1";
        } else {
            protocol = response.url().protocol();
        }
    }
    responseObject->setProtocol(protocol);

    if (response.getSecurityStyle() != ResourceResponse::SecurityStyleUnknown
        && response.getSecurityStyle() != ResourceResponse::SecurityStyleUnauthenticated) {

        const ResourceResponse::SecurityDetails* responseSecurityDetails = response.getSecurityDetails();

        int numUnknownSCTs = safeCast<int>(responseSecurityDetails->numUnknownSCTs);
        int numInvalidSCTs = safeCast<int>(responseSecurityDetails->numInvalidSCTs);
        int numValidSCTs = safeCast<int>(responseSecurityDetails->numValidSCTs);

        std::unique_ptr<protocol::Network::CertificateValidationDetails> certificateValidationDetails = protocol::Network::CertificateValidationDetails::create()
            .setNumUnknownScts(numUnknownSCTs)
            .setNumInvalidScts(numInvalidSCTs)
            .setNumValidScts(numValidSCTs).build();

        std::unique_ptr<protocol::Array<protocol::Network::SignedCertificateTimestamp>> signedCertificateTimestampList = protocol::Array<protocol::Network::SignedCertificateTimestamp>::create();
        for (auto const& sct : responseSecurityDetails->sctList) {
            std::unique_ptr<protocol::Network::SignedCertificateTimestamp> signedCertificateTimestamp = protocol::Network::SignedCertificateTimestamp::create()
                .setStatus(sct.m_status)
                .setOrigin(sct.m_origin)
                .setLogDescription(sct.m_logDescription)
                .setLogId(sct.m_logId)
                .setTimestamp(sct.m_timestamp)
                .setHashAlgorithm(sct.m_hashAlgorithm)
                .setSignatureAlgorithm(sct.m_signatureAlgorithm)
                .setSignatureData(sct.m_signatureData)
                .build();
            signedCertificateTimestampList->addItem(std::move(signedCertificateTimestamp));
        }

        std::unique_ptr<protocol::Network::SecurityDetails> securityDetails = protocol::Network::SecurityDetails::create()
            .setProtocol(responseSecurityDetails->protocol)
            .setKeyExchange(responseSecurityDetails->keyExchange)
            .setCipher(responseSecurityDetails->cipher)
            .setCertificateId(responseSecurityDetails->certID)
            .setSignedCertificateTimestampList(std::move(signedCertificateTimestampList))
            .build();
        securityDetails->setCertificateValidationDetails(std::move(certificateValidationDetails));
        if (responseSecurityDetails->mac.length() > 0)
            securityDetails->setMac(responseSecurityDetails->mac);

        responseObject->setSecurityDetails(std::move(securityDetails));
    }

    return responseObject;
}

InspectorNetworkAgent::~InspectorNetworkAgent()
{
}

DEFINE_TRACE(InspectorNetworkAgent)
{
    visitor->trace(m_inspectedFrames);
    visitor->trace(m_resourcesData);
    visitor->trace(m_replayXHRs);
    visitor->trace(m_replayXHRsToBeDeleted);
    visitor->trace(m_pendingXHRReplayData);
    InspectorBaseAgent::trace(visitor);
}

bool InspectorNetworkAgent::shouldBlockRequest(const ResourceRequest& request)
{
    protocol::DictionaryValue* blockedURLs = m_state->getObject(NetworkAgentState::blockedURLs);
    if (!blockedURLs)
        return false;

    String url = request.url().getString();
    for (size_t i = 0; i < blockedURLs->size(); ++i) {
        auto entry = blockedURLs->at(i);
        if (matches(url, entry.first))
            return true;
    }
    return false;
}

void InspectorNetworkAgent::didBlockRequest(LocalFrame* frame, const ResourceRequest& request, DocumentLoader* loader, const FetchInitiatorInfo& initiatorInfo, ResourceRequestBlockedReason reason)
{
    unsigned long identifier = createUniqueIdentifier();
    willSendRequestInternal(frame, identifier, loader, request, ResourceResponse(), initiatorInfo);

    String requestId = IdentifiersFactory::requestId(identifier);
    String protocolReason = buildBlockedReason(reason);
    frontend()->loadingFailed(requestId, monotonicallyIncreasingTime(), InspectorPageAgent::resourceTypeJson(m_resourcesData->resourceType(requestId)), String(), false, protocolReason);
}

void InspectorNetworkAgent::didChangeResourcePriority(unsigned long identifier, ResourceLoadPriority loadPriority)
{
    String requestId = IdentifiersFactory::requestId(identifier);
    frontend()->resourceChangedPriority(requestId, resourcePriorityJSON(loadPriority), monotonicallyIncreasingTime());
}

void InspectorNetworkAgent::willSendRequestInternal(LocalFrame* frame, unsigned long identifier, DocumentLoader* loader, const ResourceRequest& request, const ResourceResponse& redirectResponse, const FetchInitiatorInfo& initiatorInfo)
{
    String requestId = IdentifiersFactory::requestId(identifier);
    String loaderId = IdentifiersFactory::loaderId(loader);
    m_resourcesData->resourceCreated(requestId, loaderId, request.url());

    InspectorPageAgent::ResourceType type = InspectorPageAgent::OtherResource;
    if (initiatorInfo.name == FetchInitiatorTypeNames::xmlhttprequest) {
        type = InspectorPageAgent::XHRResource;
        m_resourcesData->setResourceType(requestId, type);
    } else if (initiatorInfo.name == FetchInitiatorTypeNames::document) {
        type = InspectorPageAgent::DocumentResource;
        m_resourcesData->setResourceType(requestId, type);
    }

    String frameId = loader->frame() ? IdentifiersFactory::frameId(loader->frame()) : "";
    std::unique_ptr<protocol::Network::Initiator> initiatorObject = buildInitiatorObject(loader->frame() ? loader->frame()->document() : 0, initiatorInfo);
    if (initiatorInfo.name == FetchInitiatorTypeNames::document) {
        FrameNavigationInitiatorMap::iterator it = m_frameNavigationInitiatorMap.find(frameId);
        if (it != m_frameNavigationInitiatorMap.end())
            initiatorObject = it->value->clone();
    }

    std::unique_ptr<protocol::Network::Request> requestInfo(buildObjectForResourceRequest(request));

    requestInfo->setMixedContentType(mixedContentTypeForContextType(MixedContentChecker::contextTypeForInspector(frame, request)));

    String resourceType = InspectorPageAgent::resourceTypeJson(type);
    frontend()->requestWillBeSent(requestId, frameId, loaderId, urlWithoutFragment(loader->url()).getString(), std::move(requestInfo), monotonicallyIncreasingTime(), currentTime(), std::move(initiatorObject), buildObjectForResourceResponse(redirectResponse), resourceType);
    if (m_pendingXHRReplayData && !m_pendingXHRReplayData->async())
        frontend()->flush();
}

void InspectorNetworkAgent::willSendRequest(LocalFrame* frame, unsigned long identifier, DocumentLoader* loader, ResourceRequest& request, const ResourceResponse& redirectResponse, const FetchInitiatorInfo& initiatorInfo)
{
    // Ignore the request initiated internally.
    if (initiatorInfo.name == FetchInitiatorTypeNames::internal)
        return;

    if (initiatorInfo.name == FetchInitiatorTypeNames::document && loader->substituteData().isValid())
        return;

    protocol::DictionaryValue* headers = m_state->getObject(NetworkAgentState::extraRequestHeaders);
    if (headers) {
        for (size_t i = 0; i < headers->size(); ++i) {
            auto header = headers->at(i);
            String16 value;
            if (header.second->asString(&value))
                request.setHTTPHeaderField(AtomicString(header.first), AtomicString(String(value)));
        }
    }

    request.setReportRawHeaders(true);

    if (m_state->booleanProperty(NetworkAgentState::cacheDisabled, false)) {
        request.setCachePolicy(WebCachePolicy::BypassingCache);
        request.setShouldResetAppCache(true);
    }
    if (m_state->booleanProperty(NetworkAgentState::bypassServiceWorker, false))
        request.setSkipServiceWorker(WebURLRequest::SkipServiceWorker::All);

    willSendRequestInternal(frame, identifier, loader, request, redirectResponse, initiatorInfo);

    if (!m_hostId.isEmpty())
        request.addHTTPHeaderField(HTTPNames::X_DevTools_Emulate_Network_Conditions_Client_Id, AtomicString(m_hostId));
}

void InspectorNetworkAgent::markResourceAsCached(unsigned long identifier)
{
    frontend()->requestServedFromCache(IdentifiersFactory::requestId(identifier));
}

void InspectorNetworkAgent::didReceiveResourceResponse(LocalFrame* frame, unsigned long identifier, DocumentLoader* loader, const ResourceResponse& response, Resource* cachedResource)
{
    String requestId = IdentifiersFactory::requestId(identifier);
    bool isNotModified = response.httpStatusCode() == 304;

    bool resourceIsEmpty = true;
    std::unique_ptr<protocol::Network::Response> resourceResponse = buildObjectForResourceResponse(response, cachedResource, &resourceIsEmpty);

    InspectorPageAgent::ResourceType type = cachedResource ? InspectorPageAgent::cachedResourceType(*cachedResource) : InspectorPageAgent::OtherResource;
    // Override with already discovered resource type.
    InspectorPageAgent::ResourceType savedType = m_resourcesData->resourceType(requestId);
    if (savedType == InspectorPageAgent::ScriptResource || savedType == InspectorPageAgent::XHRResource || savedType == InspectorPageAgent::DocumentResource
        || savedType == InspectorPageAgent::FetchResource || savedType == InspectorPageAgent::EventSourceResource) {
        type = savedType;
    }
    if (type == InspectorPageAgent::DocumentResource && loader && loader->substituteData().isValid())
        return;

    // Resources are added to NetworkResourcesData as a WeakMember here and
    // removed in willDestroyResource() called in the prefinalizer of Resource.
    // Because NetworkResourceData retains weak references only, it
    // doesn't affect Resource lifetime.
    if (cachedResource)
        m_resourcesData->addResource(requestId, cachedResource);
    String frameId = IdentifiersFactory::frameId(frame);
    String loaderId = loader ? IdentifiersFactory::loaderId(loader) : "";
    m_resourcesData->responseReceived(requestId, frameId, response);
    m_resourcesData->setResourceType(requestId, type);

    if (resourceResponse && !resourceIsEmpty)
        frontend()->responseReceived(requestId, frameId, loaderId, monotonicallyIncreasingTime(), InspectorPageAgent::resourceTypeJson(type), std::move(resourceResponse));
    // If we revalidated the resource and got Not modified, send content length following didReceiveResponse
    // as there will be no calls to didReceiveData from the network stack.
    if (isNotModified && cachedResource && cachedResource->encodedSize())
        didReceiveData(frame, identifier, 0, cachedResource->encodedSize(), 0);
}

static bool isErrorStatusCode(int statusCode)
{
    return statusCode >= 400;
}

void InspectorNetworkAgent::didReceiveData(LocalFrame*, unsigned long identifier, const char* data, int dataLength, int encodedDataLength)
{
    String requestId = IdentifiersFactory::requestId(identifier);

    if (data) {
        NetworkResourcesData::ResourceData const* resourceData = m_resourcesData->data(requestId);
        if (resourceData && (!resourceData->cachedResource() || resourceData->cachedResource()->getDataBufferingPolicy() == DoNotBufferData || isErrorStatusCode(resourceData->httpStatusCode())))
            m_resourcesData->maybeAddResourceData(requestId, data, dataLength);
    }

    frontend()->dataReceived(requestId, monotonicallyIncreasingTime(), dataLength, encodedDataLength);
}

void InspectorNetworkAgent::didFinishLoading(unsigned long identifier, double monotonicFinishTime, int64_t encodedDataLength)
{
    String requestId = IdentifiersFactory::requestId(identifier);
    m_resourcesData->maybeDecodeDataToContent(requestId);
    if (!monotonicFinishTime)
        monotonicFinishTime = monotonicallyIncreasingTime();
    frontend()->loadingFinished(requestId, monotonicFinishTime, encodedDataLength);
}

void InspectorNetworkAgent::didReceiveCORSRedirectResponse(LocalFrame* frame, unsigned long identifier, DocumentLoader* loader, const ResourceResponse& response, Resource* resource)
{
    // Update the response and finish loading
    didReceiveResourceResponse(frame, identifier, loader, response, resource);
    didFinishLoading(identifier, 0, WebURLLoaderClient::kUnknownEncodedDataLength);
}

void InspectorNetworkAgent::didFailLoading(unsigned long identifier, const ResourceError& error)
{
    String requestId = IdentifiersFactory::requestId(identifier);
    bool canceled = error.isCancellation();
    frontend()->loadingFailed(requestId, monotonicallyIncreasingTime(), InspectorPageAgent::resourceTypeJson(m_resourcesData->resourceType(requestId)), error.localizedDescription(), canceled);
}

void InspectorNetworkAgent::scriptImported(unsigned long identifier, const String& sourceString)
{
    m_resourcesData->setResourceContent(IdentifiersFactory::requestId(identifier), sourceString);
}

void InspectorNetworkAgent::didReceiveScriptResponse(unsigned long identifier)
{
    m_resourcesData->setResourceType(IdentifiersFactory::requestId(identifier), InspectorPageAgent::ScriptResource);
}

void InspectorNetworkAgent::clearPendingRequestData()
{
    if (m_pendingRequestType == InspectorPageAgent::XHRResource)
        m_pendingXHRReplayData.clear();
    m_pendingRequest = nullptr;
}

void InspectorNetworkAgent::documentThreadableLoaderStartedLoadingForClient(unsigned long identifier, ThreadableLoaderClient* client)
{
    if (!client)
        return;
    if (client != m_pendingRequest) {
        DCHECK(!m_pendingRequest);
        return;
    }

    m_knownRequestIdMap.set(client, identifier);
    String requestId = IdentifiersFactory::requestId(identifier);
    m_resourcesData->setResourceType(requestId, m_pendingRequestType);
    if (m_pendingRequestType == InspectorPageAgent::XHRResource) {
        m_resourcesData->setXHRReplayData(requestId, m_pendingXHRReplayData.get());
    }

    clearPendingRequestData();
}

void InspectorNetworkAgent::documentThreadableLoaderFailedToStartLoadingForClient(ThreadableLoaderClient* client)
{
    if (!client)
        return;
    if (client != m_pendingRequest) {
        DCHECK(!m_pendingRequest);
        return;
    }

    clearPendingRequestData();
}

void InspectorNetworkAgent::willLoadXHR(XMLHttpRequest* xhr, ThreadableLoaderClient* client, const AtomicString& method, const KURL& url, bool async, PassRefPtr<EncodedFormData> formData, const HTTPHeaderMap& headers, bool includeCredentials)
{
    DCHECK(xhr);
    DCHECK(!m_pendingRequest);
    m_pendingRequest = client;
    m_pendingRequestType = InspectorPageAgent::XHRResource;
    m_pendingXHRReplayData = XHRReplayData::create(xhr->getExecutionContext(), method, urlWithoutFragment(url), async, formData.get(), includeCredentials);
    for (const auto& header : headers)
        m_pendingXHRReplayData->addHeader(header.key, header.value);
}

void InspectorNetworkAgent::delayedRemoveReplayXHR(XMLHttpRequest* xhr)
{
    if (!m_replayXHRs.contains(xhr))
        return;

    m_replayXHRsToBeDeleted.add(xhr);
    m_replayXHRs.remove(xhr);
    m_removeFinishedReplayXHRTimer.startOneShot(0, BLINK_FROM_HERE);
}

void InspectorNetworkAgent::didFailXHRLoading(ExecutionContext* context, XMLHttpRequest* xhr, ThreadableLoaderClient* client, const AtomicString& method, const String& url)
{
    didFinishXHRInternal(context, xhr, client, method, url, false);
}

void InspectorNetworkAgent::didFinishXHRLoading(ExecutionContext* context, XMLHttpRequest* xhr, ThreadableLoaderClient* client, const AtomicString& method, const String& url)
{
    didFinishXHRInternal(context, xhr, client, method, url, true);
}

void InspectorNetworkAgent::didFinishXHRInternal(ExecutionContext* context, XMLHttpRequest* xhr, ThreadableLoaderClient* client, const AtomicString& method, const String& url, bool success)
{
    clearPendingRequestData();

    // This method will be called from the XHR.
    // We delay deleting the replay XHR, as deleting here may delete the caller.
    delayedRemoveReplayXHR(xhr);

    ThreadableLoaderClientRequestIdMap::iterator it = m_knownRequestIdMap.find(client);
    if (it == m_knownRequestIdMap.end())
        return;

    if (m_state->booleanProperty(NetworkAgentState::monitoringXHR, false)) {
        String message = (success ? "XHR finished loading: " : "XHR failed loading: ") + method + " \"" + url + "\".";
        ConsoleMessage* consoleMessage = ConsoleMessage::createForRequest(NetworkMessageSource, DebugMessageLevel, message, url, it->value);
        m_inspectedFrames->root()->console().addMessageToStorage(consoleMessage);
    }
    m_knownRequestIdMap.remove(client);
}

void InspectorNetworkAgent::willStartFetch(ThreadableLoaderClient* client)
{
    DCHECK(!m_pendingRequest);
    m_pendingRequest = client;
    m_pendingRequestType = InspectorPageAgent::FetchResource;
}

void InspectorNetworkAgent::didFailFetch(ThreadableLoaderClient* client)
{
    m_knownRequestIdMap.remove(client);
}

void InspectorNetworkAgent::didFinishFetch(ExecutionContext* context, ThreadableLoaderClient* client, const AtomicString& method, const String& url)
{
    ThreadableLoaderClientRequestIdMap::iterator it = m_knownRequestIdMap.find(client);
    if (it == m_knownRequestIdMap.end())
        return;

    if (m_state->booleanProperty(NetworkAgentState::monitoringXHR, false)) {
        String message = "Fetch complete: " + method + " \"" + url + "\".";
        ConsoleMessage* consoleMessage = ConsoleMessage::createForRequest(NetworkMessageSource, DebugMessageLevel, message, url, it->value);
        m_inspectedFrames->root()->console().addMessageToStorage(consoleMessage);
    }
    m_knownRequestIdMap.remove(client);
}

void InspectorNetworkAgent::willSendEventSourceRequest(ThreadableLoaderClient* eventSource)
{
    DCHECK(!m_pendingRequest);
    m_pendingRequest = eventSource;
    m_pendingRequestType = InspectorPageAgent::EventSourceResource;
}

void InspectorNetworkAgent::willDispatchEventSourceEvent(ThreadableLoaderClient* eventSource, const AtomicString& eventName, const AtomicString& eventId, const String& data)
{
    ThreadableLoaderClientRequestIdMap::iterator it = m_knownRequestIdMap.find(eventSource);
    if (it == m_knownRequestIdMap.end())
        return;
    frontend()->eventSourceMessageReceived(IdentifiersFactory::requestId(it->value), monotonicallyIncreasingTime(), eventName.getString(), eventId.getString(), data);
}

void InspectorNetworkAgent::didFinishEventSourceRequest(ThreadableLoaderClient* eventSource)
{
    m_knownRequestIdMap.remove(eventSource);
    clearPendingRequestData();
}

void InspectorNetworkAgent::applyUserAgentOverride(String* userAgent)
{
    String16 userAgentOverride;
    m_state->getString(NetworkAgentState::userAgentOverride, &userAgentOverride);
    if (!userAgentOverride.isEmpty())
        *userAgent = userAgentOverride;
}

void InspectorNetworkAgent::willRecalculateStyle(Document*)
{
    m_isRecalculatingStyle = true;
}

void InspectorNetworkAgent::didRecalculateStyle()
{
    m_isRecalculatingStyle = false;
    m_styleRecalculationInitiator = nullptr;
}

void InspectorNetworkAgent::didScheduleStyleRecalculation(Document* document)
{
    if (!m_styleRecalculationInitiator)
        m_styleRecalculationInitiator = buildInitiatorObject(document, FetchInitiatorInfo());
}

std::unique_ptr<protocol::Network::Initiator> InspectorNetworkAgent::buildInitiatorObject(Document* document, const FetchInitiatorInfo& initiatorInfo)
{
    std::unique_ptr<protocol::Runtime::StackTrace> currentStackTrace = SourceLocation::capture(document)->buildInspectorObject();
    if (currentStackTrace) {
        std::unique_ptr<protocol::Network::Initiator> initiatorObject = protocol::Network::Initiator::create()
            .setType(protocol::Network::Initiator::TypeEnum::Script).build();
        initiatorObject->setStack(std::move(currentStackTrace));
        return initiatorObject;
    }

    while (document && !document->scriptableDocumentParser())
        document = document->localOwner() ? document->localOwner()->ownerDocument() : nullptr;
    if (document && document->scriptableDocumentParser()) {
        std::unique_ptr<protocol::Network::Initiator> initiatorObject = protocol::Network::Initiator::create()
            .setType(protocol::Network::Initiator::TypeEnum::Parser).build();
        initiatorObject->setUrl(urlWithoutFragment(document->url()).getString());
        if (TextPosition::belowRangePosition() != initiatorInfo.position)
            initiatorObject->setLineNumber(initiatorInfo.position.m_line.oneBasedInt());
        else
            initiatorObject->setLineNumber(document->scriptableDocumentParser()->lineNumber().oneBasedInt());
        return initiatorObject;
    }

    if (m_isRecalculatingStyle && m_styleRecalculationInitiator)
        return m_styleRecalculationInitiator->clone();

    return protocol::Network::Initiator::create()
        .setType(protocol::Network::Initiator::TypeEnum::Other).build();
}

void InspectorNetworkAgent::didCreateWebSocket(Document*, unsigned long identifier, const KURL& requestURL, const String&)
{
    frontend()->webSocketCreated(IdentifiersFactory::requestId(identifier), urlWithoutFragment(requestURL).getString());
}

void InspectorNetworkAgent::willSendWebSocketHandshakeRequest(Document*, unsigned long identifier, const WebSocketHandshakeRequest* request)
{
    DCHECK(request);
    std::unique_ptr<protocol::Network::WebSocketRequest> requestObject = protocol::Network::WebSocketRequest::create()
        .setHeaders(buildObjectForHeaders(request->headerFields())).build();
    frontend()->webSocketWillSendHandshakeRequest(IdentifiersFactory::requestId(identifier), monotonicallyIncreasingTime(), currentTime(), std::move(requestObject));
}

void InspectorNetworkAgent::didReceiveWebSocketHandshakeResponse(Document*, unsigned long identifier, const WebSocketHandshakeRequest* request, const WebSocketHandshakeResponse* response)
{
    DCHECK(response);
    std::unique_ptr<protocol::Network::WebSocketResponse> responseObject = protocol::Network::WebSocketResponse::create()
        .setStatus(response->statusCode())
        .setStatusText(response->statusText())
        .setHeaders(buildObjectForHeaders(response->headerFields())).build();

    if (!response->headersText().isEmpty())
        responseObject->setHeadersText(response->headersText());
    if (request) {
        responseObject->setRequestHeaders(buildObjectForHeaders(request->headerFields()));
        if (!request->headersText().isEmpty())
            responseObject->setRequestHeadersText(request->headersText());
    }
    frontend()->webSocketHandshakeResponseReceived(IdentifiersFactory::requestId(identifier), monotonicallyIncreasingTime(), std::move(responseObject));
}

void InspectorNetworkAgent::didCloseWebSocket(Document*, unsigned long identifier)
{
    frontend()->webSocketClosed(IdentifiersFactory::requestId(identifier), monotonicallyIncreasingTime());
}

void InspectorNetworkAgent::didReceiveWebSocketFrame(unsigned long identifier, int opCode, bool masked, const char* payload, size_t payloadLength)
{
    std::unique_ptr<protocol::Network::WebSocketFrame> frameObject = protocol::Network::WebSocketFrame::create()
        .setOpcode(opCode)
        .setMask(masked)
        .setPayloadData(String::fromUTF8WithLatin1Fallback(payload, payloadLength)).build();
    frontend()->webSocketFrameReceived(IdentifiersFactory::requestId(identifier), monotonicallyIncreasingTime(), std::move(frameObject));
}

void InspectorNetworkAgent::didSendWebSocketFrame(unsigned long identifier, int opCode, bool masked, const char* payload, size_t payloadLength)
{
    std::unique_ptr<protocol::Network::WebSocketFrame> frameObject = protocol::Network::WebSocketFrame::create()
        .setOpcode(opCode)
        .setMask(masked)
        .setPayloadData(String::fromUTF8WithLatin1Fallback(payload, payloadLength)).build();
    frontend()->webSocketFrameSent(IdentifiersFactory::requestId(identifier), monotonicallyIncreasingTime(), std::move(frameObject));
}

void InspectorNetworkAgent::didReceiveWebSocketFrameError(unsigned long identifier, const String& errorMessage)
{
    frontend()->webSocketFrameError(IdentifiersFactory::requestId(identifier), monotonicallyIncreasingTime(), errorMessage);
}

void InspectorNetworkAgent::enable(ErrorString*, const Maybe<int>& totalBufferSize, const Maybe<int>& resourceBufferSize)
{
    enable(totalBufferSize.fromMaybe(maximumTotalBufferSize), resourceBufferSize.fromMaybe(maximumResourceBufferSize));
}

void InspectorNetworkAgent::enable(int totalBufferSize, int resourceBufferSize)
{
    if (!frontend())
        return;
    m_resourcesData->setResourcesDataSizeLimits(totalBufferSize, resourceBufferSize);
    m_state->setBoolean(NetworkAgentState::networkAgentEnabled, true);
    m_state->setNumber(NetworkAgentState::totalBufferSize, totalBufferSize);
    m_state->setNumber(NetworkAgentState::resourceBufferSize, resourceBufferSize);
    m_instrumentingAgents->addInspectorNetworkAgent(this);
}

void InspectorNetworkAgent::disable(ErrorString*)
{
    DCHECK(!m_pendingRequest);
    m_state->setBoolean(NetworkAgentState::networkAgentEnabled, false);
    m_state->setString(NetworkAgentState::userAgentOverride, "");
    m_instrumentingAgents->removeInspectorNetworkAgent(this);
    m_resourcesData->clear();
    m_knownRequestIdMap.clear();
}

void InspectorNetworkAgent::setUserAgentOverride(ErrorString* errorString, const String& userAgent)
{
    if (userAgent.contains('\n') || userAgent.contains('\r') || userAgent.contains('\0')) {
        *errorString = "Invalid characters found in userAgent";
        return;
    }
    m_state->setString(NetworkAgentState::userAgentOverride, userAgent);
}

void InspectorNetworkAgent::setExtraHTTPHeaders(ErrorString*, const std::unique_ptr<protocol::Network::Headers> headers)
{
    m_state->setObject(NetworkAgentState::extraRequestHeaders, headers->serialize());
}

bool InspectorNetworkAgent::canGetResponseBodyBlob(const String& requestId)
{
    NetworkResourcesData::ResourceData const* resourceData = m_resourcesData->data(requestId);
    BlobDataHandle* blob = resourceData ? resourceData->downloadedFileBlob() : nullptr;
    if (!blob)
        return false;
    LocalFrame* frame = IdentifiersFactory::frameById(m_inspectedFrames, resourceData->frameId());
    return frame && frame->document();
}

void InspectorNetworkAgent::getResponseBodyBlob(const String& requestId, std::unique_ptr<GetResponseBodyCallback> callback)
{
    NetworkResourcesData::ResourceData const* resourceData = m_resourcesData->data(requestId);
    BlobDataHandle* blob = resourceData->downloadedFileBlob();
    LocalFrame* frame = IdentifiersFactory::frameById(m_inspectedFrames, resourceData->frameId());
    Document* document = frame->document();
    InspectorFileReaderLoaderClient* client = new InspectorFileReaderLoaderClient(blob, resourceData->mimeType(), resourceData->textEncodingName(), std::move(callback));
    client->start(document);
}

void InspectorNetworkAgent::getResponseBody(ErrorString* errorString, const String& requestId, std::unique_ptr<GetResponseBodyCallback> passCallback)
{
    std::unique_ptr<GetResponseBodyCallback> callback = std::move(passCallback);
    NetworkResourcesData::ResourceData const* resourceData = m_resourcesData->data(requestId);
    if (!resourceData) {
        callback->sendFailure("No resource with given identifier found");
        return;
    }

    // XHR with ResponseTypeBlob should be returned as blob.
    if (resourceData->xhrReplayData() && canGetResponseBodyBlob(requestId)) {
        getResponseBodyBlob(requestId, std::move(callback));
        return;
    }

    if (resourceData->hasContent()) {
        callback->sendSuccess(resourceData->content(), resourceData->base64Encoded());
        return;
    }

    if (resourceData->isContentEvicted()) {
        callback->sendFailure("Request content was evicted from inspector cache");
        return;
    }

    if (resourceData->buffer() && !resourceData->textEncodingName().isNull()) {
        String result;
        bool base64Encoded;
        bool success = InspectorPageAgent::sharedBufferContent(resourceData->buffer(), resourceData->mimeType(), resourceData->textEncodingName(), &result, &base64Encoded);
        DCHECK(success);
        callback->sendSuccess(result, base64Encoded);
        return;
    }

    if (resourceData->cachedResource()) {
        String content;
        bool base64Encoded = false;
        if (InspectorPageAgent::cachedResourceContent(resourceData->cachedResource(), &content, &base64Encoded)) {
            callback->sendSuccess(content, base64Encoded);
            return;
        }
    }

    if (canGetResponseBodyBlob(requestId)) {
        getResponseBodyBlob(requestId, std::move(callback));
        return;
    }

    callback->sendFailure("No data found for resource with given identifier");
}

void InspectorNetworkAgent::addBlockedURL(ErrorString*, const String& url)
{
    protocol::DictionaryValue* blockedURLs = m_state->getObject(NetworkAgentState::blockedURLs);
    if (!blockedURLs) {
        std::unique_ptr<protocol::DictionaryValue> newList = protocol::DictionaryValue::create();
        blockedURLs = newList.get();
        m_state->setObject(NetworkAgentState::blockedURLs, std::move(newList));
    }
    blockedURLs->setBoolean(url, true);
}

void InspectorNetworkAgent::removeBlockedURL(ErrorString*, const String& url)
{
    protocol::DictionaryValue* blockedURLs = m_state->getObject(NetworkAgentState::blockedURLs);
    if (blockedURLs)
        blockedURLs->remove(url);
}

void InspectorNetworkAgent::replayXHR(ErrorString*, const String& requestId)
{
    String actualRequestId = requestId;

    XHRReplayData* xhrReplayData = m_resourcesData->xhrReplayData(requestId);
    if (!xhrReplayData)
        return;

    ExecutionContext* executionContext = xhrReplayData->getExecutionContext();
    if (!executionContext) {
        m_resourcesData->setXHRReplayData(requestId, 0);
        return;
    }

    XMLHttpRequest* xhr = XMLHttpRequest::create(executionContext);

    executionContext->removeURLFromMemoryCache(xhrReplayData->url());

    xhr->open(xhrReplayData->method(), xhrReplayData->url(), xhrReplayData->async(), IGNORE_EXCEPTION);
    if (xhrReplayData->includeCredentials())
        xhr->setWithCredentials(true, IGNORE_EXCEPTION);
    for (const auto& header : xhrReplayData->headers())
        xhr->setRequestHeader(header.key, header.value, IGNORE_EXCEPTION);
    xhr->sendForInspectorXHRReplay(xhrReplayData->formData(), IGNORE_EXCEPTION);

    m_replayXHRs.add(xhr);
}

void InspectorNetworkAgent::setMonitoringXHREnabled(ErrorString*, bool enabled)
{
    m_state->setBoolean(NetworkAgentState::monitoringXHR, enabled);
}

void InspectorNetworkAgent::canClearBrowserCache(ErrorString*, bool* result)
{
    *result = true;
}

void InspectorNetworkAgent::canClearBrowserCookies(ErrorString*, bool* result)
{
    *result = true;
}

void InspectorNetworkAgent::emulateNetworkConditions(ErrorString* errorString, bool offline, double latency, double downloadThroughput, double uploadThroughput, const Maybe<String>& connectionType)
{
    WebConnectionType type = WebConnectionTypeUnknown;
    if (connectionType.isJust()) {
        type = toWebConnectionType(errorString, connectionType.fromJust());
        if (!errorString->isEmpty())
            return;
    }
    // TODO(dgozman): networkStateNotifier is per-process. It would be nice to have per-frame override instead.
    if (offline || latency || downloadThroughput || uploadThroughput)
        networkStateNotifier().setOverride(!offline, type, downloadThroughput / (1024 * 1024 / 8));
    else
        networkStateNotifier().clearOverride();
}

void InspectorNetworkAgent::setCacheDisabled(ErrorString*, bool cacheDisabled)
{
    m_state->setBoolean(NetworkAgentState::cacheDisabled, cacheDisabled);
    if (cacheDisabled)
        memoryCache()->evictResources();
}

void InspectorNetworkAgent::setBypassServiceWorker(ErrorString*, bool bypass)
{
    m_state->setBoolean(NetworkAgentState::bypassServiceWorker, bypass);
}

void InspectorNetworkAgent::setDataSizeLimitsForTest(ErrorString*, int maxTotal, int maxResource)
{
    m_resourcesData->setResourcesDataSizeLimits(maxTotal, maxResource);
}

void InspectorNetworkAgent::didCommitLoad(LocalFrame* frame, DocumentLoader* loader)
{
    if (loader->frame() != m_inspectedFrames->root())
        return;

    if (m_state->booleanProperty(NetworkAgentState::cacheDisabled, false))
        memoryCache()->evictResources();

    m_resourcesData->clear(IdentifiersFactory::loaderId(loader));
}

void InspectorNetworkAgent::frameScheduledNavigation(LocalFrame* frame, double)
{
    std::unique_ptr<protocol::Network::Initiator> initiator = buildInitiatorObject(frame->document(), FetchInitiatorInfo());
    m_frameNavigationInitiatorMap.set(IdentifiersFactory::frameId(frame), std::move(initiator));
}

void InspectorNetworkAgent::frameClearedScheduledNavigation(LocalFrame* frame)
{
    m_frameNavigationInitiatorMap.remove(IdentifiersFactory::frameId(frame));
}

void InspectorNetworkAgent::setHostId(const String& hostId)
{
    m_hostId = hostId;
}

bool InspectorNetworkAgent::fetchResourceContent(Document* document, const KURL& url, String* content, bool* base64Encoded)
{
    // First try to fetch content from the cached resource.
    Resource* cachedResource = document->fetcher()->cachedResource(url);
    if (!cachedResource)
        cachedResource = memoryCache()->resourceForURL(url, document->fetcher()->getCacheIdentifier());
    if (cachedResource && InspectorPageAgent::cachedResourceContent(cachedResource, content, base64Encoded))
        return true;

    // Then fall back to resource data.
    for (auto& resource : m_resourcesData->resources()) {
        if (resource->requestedURL() == url) {
            *content = resource->content();
            *base64Encoded = resource->base64Encoded();
            return true;
        }
    }
    return false;
}

void InspectorNetworkAgent::removeFinishedReplayXHRFired(Timer<InspectorNetworkAgent>*)
{
    m_replayXHRsToBeDeleted.clear();
}

InspectorNetworkAgent::InspectorNetworkAgent(InspectedFrames* inspectedFrames)
    : m_inspectedFrames(inspectedFrames)
    , m_resourcesData(NetworkResourcesData::create(maximumTotalBufferSize, maximumResourceBufferSize))
    , m_pendingRequest(nullptr)
    , m_isRecalculatingStyle(false)
    , m_removeFinishedReplayXHRTimer(this, &InspectorNetworkAgent::removeFinishedReplayXHRFired)
{
}

bool InspectorNetworkAgent::shouldForceCORSPreflight()
{
    return m_state->booleanProperty(NetworkAgentState::cacheDisabled, false);
}

} // namespace blink
