/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
 *
 */

#include "core/loader/PingLoader.h"

#include "core/dom/DOMArrayBufferView.h"
#include "core/dom/Document.h"
#include "core/dom/SecurityContext.h"
#include "core/fetch/CrossOriginAccessControl.h"
#include "core/fetch/FetchContext.h"
#include "core/fetch/FetchInitiatorTypeNames.h"
#include "core/fetch/FetchUtils.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/fetch/UniqueIdentifier.h"
#include "core/fileapi/File.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/FormData.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/MixedContentChecker.h"
#include "core/page/Page.h"
#include "platform/exported/WrappedResourceRequest.h"
#include "platform/exported/WrappedResourceResponse.h"
#include "platform/network/EncodedFormData.h"
#include "platform/network/ParsedContentType.h"
#include "platform/network/ResourceError.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "public/platform/Platform.h"
#include "public/platform/WebFrameScheduler.h"
#include "public/platform/WebURLLoader.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/WebURLResponse.h"
#include "wtf/Compiler.h"
#include "wtf/Functional.h"
#include "wtf/PtrUtil.h"

namespace blink {

namespace {

class Beacon {
  STACK_ALLOCATED();

 public:
  virtual void serialize(ResourceRequest&) const = 0;
  virtual unsigned long long size() const = 0;
  virtual const AtomicString getContentType() const = 0;
};

class BeaconString final : public Beacon {
 public:
  explicit BeaconString(const String& data) : m_data(data) {}

  unsigned long long size() const override {
    return m_data.charactersSizeInBytes();
  }

  void serialize(ResourceRequest& request) const override {
    RefPtr<EncodedFormData> entityBody = EncodedFormData::create(m_data.utf8());
    request.setHTTPBody(entityBody);
    request.setHTTPContentType(getContentType());
  }

  const AtomicString getContentType() const {
    return AtomicString("text/plain;charset=UTF-8");
  }

 private:
  const String m_data;
};

class BeaconBlob final : public Beacon {
 public:
  explicit BeaconBlob(Blob* data) : m_data(data) {
    const String& blobType = m_data->type();
    if (!blobType.isEmpty() && isValidContentType(blobType))
      m_contentType = AtomicString(blobType);
  }

  unsigned long long size() const override { return m_data->size(); }

  void serialize(ResourceRequest& request) const override {
    DCHECK(m_data);

    RefPtr<EncodedFormData> entityBody = EncodedFormData::create();
    if (m_data->hasBackingFile())
      entityBody->appendFile(toFile(m_data)->path());
    else
      entityBody->appendBlob(m_data->uuid(), m_data->blobDataHandle());

    request.setHTTPBody(entityBody.release());

    if (!m_contentType.isEmpty())
      request.setHTTPContentType(m_contentType);
  }

  const AtomicString getContentType() const { return m_contentType; }

 private:
  const Member<Blob> m_data;
  AtomicString m_contentType;
};

class BeaconDOMArrayBufferView final : public Beacon {
 public:
  explicit BeaconDOMArrayBufferView(DOMArrayBufferView* data) : m_data(data) {}

  unsigned long long size() const override { return m_data->byteLength(); }

  void serialize(ResourceRequest& request) const override {
    DCHECK(m_data);

    RefPtr<EncodedFormData> entityBody =
        EncodedFormData::create(m_data->baseAddress(), m_data->byteLength());
    request.setHTTPBody(entityBody.release());

    // FIXME: a reasonable choice, but not in the spec; should it give a
    // default?
    request.setHTTPContentType(AtomicString("application/octet-stream"));
  }

  const AtomicString getContentType() const { return nullAtom; }

 private:
  const Member<DOMArrayBufferView> m_data;
};

class BeaconFormData final : public Beacon {
 public:
  explicit BeaconFormData(FormData* data)
      : m_data(data), m_entityBody(m_data->encodeMultiPartFormData()) {
    m_contentType = AtomicString("multipart/form-data; boundary=") +
                    m_entityBody->boundary().data();
  }

  unsigned long long size() const override {
    return m_entityBody->sizeInBytes();
  }

  void serialize(ResourceRequest& request) const override {
    request.setHTTPBody(m_entityBody.get());
    request.setHTTPContentType(m_contentType);
  }

  const AtomicString getContentType() const { return m_contentType; }

 private:
  const Member<FormData> m_data;
  RefPtr<EncodedFormData> m_entityBody;
  AtomicString m_contentType;
};

class PingLoaderImpl : public GarbageCollectedFinalized<PingLoaderImpl>,
                       public DOMWindowProperty,
                       private WebURLLoaderClient {
  USING_GARBAGE_COLLECTED_MIXIN(PingLoaderImpl);
  WTF_MAKE_NONCOPYABLE(PingLoaderImpl);

 public:
  PingLoaderImpl(LocalFrame*,
                 ResourceRequest&,
                 const AtomicString&,
                 StoredCredentials,
                 bool);
  ~PingLoaderImpl() override;

  DECLARE_VIRTUAL_TRACE();

 private:
  void dispose();

  // WebURLLoaderClient
  bool willFollowRedirect(WebURLLoader*,
                          WebURLRequest&,
                          const WebURLResponse&) override;
  void didReceiveResponse(WebURLLoader*, const WebURLResponse&) final;
  void didReceiveData(WebURLLoader*, const char*, int, int, int) final;
  void didFinishLoading(WebURLLoader*, double, int64_t) final;
  void didFail(WebURLLoader*, const WebURLError&) final;

  void timeout(TimerBase*);

  void didFailLoading(LocalFrame*);

  std::unique_ptr<WebURLLoader> m_loader;
  Timer<PingLoaderImpl> m_timeout;
  String m_url;
  unsigned long m_identifier;
  SelfKeepAlive<PingLoaderImpl> m_keepAlive;

  bool m_isBeacon;

  RefPtr<SecurityOrigin> m_origin;
  CORSEnabled m_corsMode;
};

PingLoaderImpl::PingLoaderImpl(LocalFrame* frame,
                               ResourceRequest& request,
                               const AtomicString& initiator,
                               StoredCredentials credentialsAllowed,
                               bool isBeacon)
    : DOMWindowProperty(frame),
      m_timeout(this, &PingLoaderImpl::timeout),
      m_url(request.url()),
      m_identifier(createUniqueIdentifier()),
      m_keepAlive(this),
      m_isBeacon(isBeacon),
      m_origin(frame->document()->getSecurityOrigin()),
      m_corsMode(IsCORSEnabled) {
  const AtomicString contentType = request.httpContentType();
  if (!contentType.isNull() &&
      FetchUtils::isSimpleHeader(AtomicString("content-type"), contentType))
    m_corsMode = NotCORSEnabled;

  frame->loader().client()->didDispatchPingLoader(request.url());

  FetchContext& fetchContext = frame->document()->fetcher()->context();

  fetchContext.willStartLoadingResource(m_identifier, request, Resource::Image);

  FetchInitiatorInfo initiatorInfo;
  initiatorInfo.name = initiator;
  fetchContext.dispatchWillSendRequest(m_identifier, request,
                                       ResourceResponse(), initiatorInfo);

  // Make sure the scheduler doesn't wait for the ping.
  if (frame->frameScheduler())
    frame->frameScheduler()->didStopLoading(m_identifier);

  m_loader = wrapUnique(Platform::current()->createURLLoader());
  DCHECK(m_loader);
  WrappedResourceRequest wrappedRequest(request);
  wrappedRequest.setAllowStoredCredentials(credentialsAllowed ==
                                           AllowStoredCredentials);
  m_loader->loadAsynchronously(wrappedRequest, this);

  // If the server never responds, FrameLoader won't be able to cancel this load
  // and we'll sit here waiting forever. Set a very generous timeout, just in
  // case.
  m_timeout.startOneShot(60000, BLINK_FROM_HERE);
}

PingLoaderImpl::~PingLoaderImpl() {
  if (m_loader)
    m_loader->cancel();
}

void PingLoaderImpl::dispose() {
  if (m_loader) {
    m_loader->cancel();
    m_loader = nullptr;
  }
  m_timeout.stop();
  m_keepAlive.clear();
}

bool PingLoaderImpl::willFollowRedirect(
    WebURLLoader*,
    WebURLRequest& passedNewRequest,
    const WebURLResponse& passedRedirectResponse) {
  if (!m_isBeacon)
    return true;

  if (m_corsMode == NotCORSEnabled)
    return true;

  DCHECK(passedNewRequest.allowStoredCredentials());

  ResourceRequest& newRequest(passedNewRequest.toMutableResourceRequest());
  const ResourceResponse& redirectResponse(
      passedRedirectResponse.toResourceResponse());

  DCHECK(!newRequest.isNull());
  DCHECK(!redirectResponse.isNull());

  String errorDescription;
  ResourceLoaderOptions options;
  // TODO(tyoshino): Save updated data in options.securityOrigin and pass it
  // on the next time.
  if (!CrossOriginAccessControl::handleRedirect(
          m_origin, newRequest, redirectResponse, AllowStoredCredentials,
          options, errorDescription)) {
    if (LocalFrame* localFrame = frame()) {
      if (localFrame->document()) {
        localFrame->document()->addConsoleMessage(ConsoleMessage::create(
            JSMessageSource, ErrorMessageLevel, errorDescription));
      }
    }
    // Cancel the load and self destruct.
    dispose();

    return false;
  }
  // FIXME: http://crbug.com/427429 is needed to correctly propagate updates of
  // Origin: following this successful redirect.

  return true;
}

void PingLoaderImpl::didReceiveResponse(WebURLLoader*,
                                        const WebURLResponse& response) {
  if (LocalFrame* frame = this->frame()) {
    TRACE_EVENT_INSTANT1(
        "devtools.timeline", "ResourceFinish", TRACE_EVENT_SCOPE_THREAD, "data",
        InspectorResourceFinishEvent::data(m_identifier, 0, true));
    const ResourceResponse& resourceResponse = response.toResourceResponse();
    InspectorInstrumentation::didReceiveResourceResponse(frame, m_identifier, 0,
                                                         resourceResponse, 0);
    didFailLoading(frame);
  }
  dispose();
}

void PingLoaderImpl::didReceiveData(WebURLLoader*, const char*, int, int, int) {
  if (LocalFrame* frame = this->frame()) {
    TRACE_EVENT_INSTANT1(
        "devtools.timeline", "ResourceFinish", TRACE_EVENT_SCOPE_THREAD, "data",
        InspectorResourceFinishEvent::data(m_identifier, 0, true));
    didFailLoading(frame);
  }
  dispose();
}

void PingLoaderImpl::didFinishLoading(WebURLLoader*, double, int64_t) {
  if (LocalFrame* frame = this->frame()) {
    TRACE_EVENT_INSTANT1(
        "devtools.timeline", "ResourceFinish", TRACE_EVENT_SCOPE_THREAD, "data",
        InspectorResourceFinishEvent::data(m_identifier, 0, true));
    didFailLoading(frame);
  }
  dispose();
}

void PingLoaderImpl::didFail(WebURLLoader*, const WebURLError& resourceError) {
  if (LocalFrame* frame = this->frame()) {
    TRACE_EVENT_INSTANT1(
        "devtools.timeline", "ResourceFinish", TRACE_EVENT_SCOPE_THREAD, "data",
        InspectorResourceFinishEvent::data(m_identifier, 0, true));
    didFailLoading(frame);
  }
  dispose();
}

void PingLoaderImpl::timeout(TimerBase*) {
  if (LocalFrame* frame = this->frame()) {
    TRACE_EVENT_INSTANT1(
        "devtools.timeline", "ResourceFinish", TRACE_EVENT_SCOPE_THREAD, "data",
        InspectorResourceFinishEvent::data(m_identifier, 0, true));
    didFailLoading(frame);
  }
  dispose();
}

void PingLoaderImpl::didFailLoading(LocalFrame* frame) {
  InspectorInstrumentation::didFailLoading(
      frame, m_identifier, ResourceError::cancelledError(m_url));
  frame->console().didFailLoading(m_identifier,
                                  ResourceError::cancelledError(m_url));
}

DEFINE_TRACE(PingLoaderImpl) {
  DOMWindowProperty::trace(visitor);
}

void finishPingRequestInitialization(
    ResourceRequest& request,
    LocalFrame* frame,
    WebURLRequest::RequestContext requestContext) {
  request.setRequestContext(requestContext);
  FetchContext& fetchContext = frame->document()->fetcher()->context();
  fetchContext.addAdditionalRequestHeaders(request, FetchSubresource);
  fetchContext.populateRequestData(request);
}

bool sendPingCommon(LocalFrame* frame,
                    ResourceRequest& request,
                    const AtomicString& initiator,
                    StoredCredentials credentialsAllowed,
                    bool isBeacon) {
  if (MixedContentChecker::shouldBlockFetch(frame, request, request.url()))
    return false;

  // The loader keeps itself alive until it receives a response and disposes
  // itself.
  PingLoaderImpl* loader =
      new PingLoaderImpl(frame, request, initiator, credentialsAllowed, true);
  DCHECK(loader);

  return true;
}

bool sendBeaconCommon(LocalFrame* frame,
                      int allowance,
                      const KURL& url,
                      const Beacon& beacon,
                      int& payloadLength) {
  if (!frame->document())
    return false;

  unsigned long long entitySize = beacon.size();
  if (allowance > 0 && static_cast<unsigned long long>(allowance) < entitySize)
    return false;

  payloadLength = entitySize;

  ResourceRequest request(url);
  request.setHTTPMethod(HTTPNames::POST);
  request.setHTTPHeaderField(HTTPNames::Cache_Control, "max-age=0");
  finishPingRequestInitialization(request, frame,
                                  WebURLRequest::RequestContextBeacon);

  beacon.serialize(request);

  return sendPingCommon(frame, request, FetchInitiatorTypeNames::beacon,
                        AllowStoredCredentials, true);
}

}  // namespace

void PingLoader::loadImage(LocalFrame* frame, const KURL& url) {
  if (!frame->document()->getSecurityOrigin()->canDisplay(url)) {
    FrameLoader::reportLocalLoadFailed(frame, url.getString());
    return;
  }

  ResourceRequest request(url);
  request.setHTTPHeaderField(HTTPNames::Cache_Control, "max-age=0");
  finishPingRequestInitialization(request, frame,
                                  WebURLRequest::RequestContextPing);

  sendPingCommon(frame, request, FetchInitiatorTypeNames::ping,
                 AllowStoredCredentials, false);
}

// http://www.whatwg.org/specs/web-apps/current-work/multipage/links.html#hyperlink-auditing
void PingLoader::sendLinkAuditPing(LocalFrame* frame,
                                   const KURL& pingURL,
                                   const KURL& destinationURL) {
  if (!pingURL.protocolIsInHTTPFamily())
    return;

  if (ContentSecurityPolicy* policy =
          frame->securityContext()->contentSecurityPolicy()) {
    if (!policy->allowConnectToSource(pingURL))
      return;
  }

  ResourceRequest request(pingURL);
  request.setHTTPMethod(HTTPNames::POST);
  request.setHTTPContentType("text/ping");
  request.setHTTPBody(EncodedFormData::create("PING"));
  request.setHTTPHeaderField(HTTPNames::Cache_Control, "max-age=0");
  finishPingRequestInitialization(request, frame,
                                  WebURLRequest::RequestContextPing);

  // addAdditionalRequestHeaders() will have added a referrer for same origin
  // requests, but the spec omits the referrer.
  request.clearHTTPReferrer();

  request.setHTTPHeaderField(HTTPNames::Ping_To,
                             AtomicString(destinationURL.getString()));

  RefPtr<SecurityOrigin> pingOrigin = SecurityOrigin::create(pingURL);
  if (protocolIs(frame->document()->url().getString(), "http") ||
      frame->document()->getSecurityOrigin()->canAccess(pingOrigin.get())) {
    request.setHTTPHeaderField(
        HTTPNames::Ping_From,
        AtomicString(frame->document()->url().getString()));
  }

  sendPingCommon(frame, request, FetchInitiatorTypeNames::ping,
                 AllowStoredCredentials, false);
}

void PingLoader::sendViolationReport(LocalFrame* frame,
                                     const KURL& reportURL,
                                     PassRefPtr<EncodedFormData> report,
                                     ViolationReportType type) {
  ResourceRequest request(reportURL);
  request.setHTTPMethod(HTTPNames::POST);
  switch (type) {
    case ContentSecurityPolicyViolationReport:
      request.setHTTPContentType("application/csp-report");
      break;
    case XSSAuditorViolationReport:
      request.setHTTPContentType("application/xss-auditor-report");
      break;
  }
  request.setHTTPBody(std::move(report));
  finishPingRequestInitialization(request, frame,
                                  WebURLRequest::RequestContextPing);

  StoredCredentials credentialsAllowed =
      SecurityOrigin::create(reportURL)->isSameSchemeHostPort(
          frame->document()->getSecurityOrigin())
          ? AllowStoredCredentials
          : DoNotAllowStoredCredentials;
  sendPingCommon(frame, request, FetchInitiatorTypeNames::violationreport,
                 credentialsAllowed, false);
}

bool PingLoader::sendBeacon(LocalFrame* frame,
                            int allowance,
                            const KURL& beaconURL,
                            const String& data,
                            int& payloadLength) {
  BeaconString beacon(data);
  return sendBeaconCommon(frame, allowance, beaconURL, beacon, payloadLength);
}

bool PingLoader::sendBeacon(LocalFrame* frame,
                            int allowance,
                            const KURL& beaconURL,
                            DOMArrayBufferView* data,
                            int& payloadLength) {
  BeaconDOMArrayBufferView beacon(data);
  return sendBeaconCommon(frame, allowance, beaconURL, beacon, payloadLength);
}

bool PingLoader::sendBeacon(LocalFrame* frame,
                            int allowance,
                            const KURL& beaconURL,
                            FormData* data,
                            int& payloadLength) {
  BeaconFormData beacon(data);
  return sendBeaconCommon(frame, allowance, beaconURL, beacon, payloadLength);
}

bool PingLoader::sendBeacon(LocalFrame* frame,
                            int allowance,
                            const KURL& beaconURL,
                            Blob* data,
                            int& payloadLength) {
  BeaconBlob beacon(data);
  return sendBeaconCommon(frame, allowance, beaconURL, beacon, payloadLength);
}

}  // namespace blink
