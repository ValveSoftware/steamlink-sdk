// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/FetchManager.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/Document.h"
#include "core/fetch/FetchUtils.h"
#include "core/fileapi/Blob.h"
#include "core/frame/Frame.h"
#include "core/frame/SubresourceIntegrity.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/ThreadableLoader.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "modules/fetch/Body.h"
#include "modules/fetch/BodyStreamBuffer.h"
#include "modules/fetch/BytesConsumer.h"
#include "modules/fetch/BytesConsumerForDataConsumerHandle.h"
#include "modules/fetch/FetchRequestData.h"
#include "modules/fetch/FormDataBytesConsumer.h"
#include "modules/fetch/Response.h"
#include "modules/fetch/ResponseInit.h"
#include "platform/HTTPNames.h"
#include "platform/network/NetworkUtils.h"
#include "platform/network/ResourceError.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"
#include "platform/weborigin/SchemeRegistry.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "platform/weborigin/Suborigin.h"
#include "public/platform/WebURLRequest.h"
#include "wtf/HashSet.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"
#include <memory>

namespace blink {

namespace {

class SRIBytesConsumer final : public BytesConsumer {
 public:
  // BytesConsumer implementation
  Result beginRead(const char** buffer, size_t* available) override {
    if (!m_underlying) {
      *buffer = nullptr;
      *available = 0;
      return m_isCancelled ? Result::Done : Result::ShouldWait;
    }
    return m_underlying->beginRead(buffer, available);
  }
  Result endRead(size_t readSize) override {
    DCHECK(m_underlying);
    return m_underlying->endRead(readSize);
  }
  PassRefPtr<BlobDataHandle> drainAsBlobDataHandle(
      BlobSizePolicy policy) override {
    return m_underlying ? m_underlying->drainAsBlobDataHandle(policy) : nullptr;
  }
  PassRefPtr<EncodedFormData> drainAsFormData() override {
    return m_underlying ? m_underlying->drainAsFormData() : nullptr;
  }
  void setClient(BytesConsumer::Client* client) override {
    DCHECK(!m_client);
    DCHECK(client);
    if (m_underlying)
      m_underlying->setClient(client);
    else
      m_client = client;
  }
  void clearClient() override {
    if (m_underlying)
      m_underlying->clearClient();
    else
      m_client = nullptr;
  }
  void cancel() override {
    if (m_underlying) {
      m_underlying->cancel();
    } else {
      m_isCancelled = true;
      m_client = nullptr;
    }
  }
  PublicState getPublicState() const override {
    return m_underlying ? m_underlying->getPublicState()
                        : m_isCancelled ? PublicState::Closed
                                        : PublicState::ReadableOrWaiting;
  }
  Error getError() const override {
    DCHECK(m_underlying);
    // We must not be in the errored state until we get updated.
    return m_underlying->getError();
  }
  String debugName() const override { return "SRIBytesConsumer"; }

  // This function can be called at most once.
  void update(BytesConsumer* consumer) {
    DCHECK(!m_underlying);
    if (m_isCancelled) {
      // This consumer has already been closed.
      return;
    }

    m_underlying = consumer;
    if (m_client) {
      Client* client = m_client;
      m_client = nullptr;
      m_underlying->setClient(client);
      if (getPublicState() != PublicState::ReadableOrWaiting)
        client->onStateChange();
    }
  }

  DEFINE_INLINE_TRACE() {
    visitor->trace(m_underlying);
    visitor->trace(m_client);
    BytesConsumer::trace(visitor);
  }

 private:
  Member<BytesConsumer> m_underlying;
  Member<Client> m_client;
  bool m_isCancelled = false;
};

}  // namespace

class FetchManager::Loader final
    : public GarbageCollectedFinalized<FetchManager::Loader>,
      public ThreadableLoaderClient {
  USING_PRE_FINALIZER(FetchManager::Loader, dispose);

 public:
  static Loader* create(ExecutionContext* executionContext,
                        FetchManager* fetchManager,
                        ScriptPromiseResolver* resolver,
                        FetchRequestData* request,
                        bool isIsolatedWorld) {
    return new Loader(executionContext, fetchManager, resolver, request,
                      isIsolatedWorld);
  }

  ~Loader() override;
  DECLARE_VIRTUAL_TRACE();

  void didReceiveResponse(unsigned long,
                          const ResourceResponse&,
                          std::unique_ptr<WebDataConsumerHandle>) override;
  void didFinishLoading(unsigned long, double) override;
  void didFail(const ResourceError&) override;
  void didFailAccessControlCheck(const ResourceError&) override;
  void didFailRedirectCheck() override;

  void start();
  void dispose();

  class SRIVerifier final : public GarbageCollectedFinalized<SRIVerifier>,
                            public WebDataConsumerHandle::Client {
   public:
    // Promptly clear m_handle and m_reader.
    EAGERLY_FINALIZE();
    // SRIVerifier takes ownership of |handle| and |response|.
    // |updater| must be garbage collected. The other arguments
    // all must have the lifetime of the give loader.
    SRIVerifier(std::unique_ptr<WebDataConsumerHandle> handle,
                SRIBytesConsumer* updater,
                Response* response,
                FetchManager::Loader* loader,
                String integrityMetadata,
                const KURL& url)
        : m_handle(std::move(handle)),
          m_updater(updater),
          m_response(response),
          m_loader(loader),
          m_integrityMetadata(integrityMetadata),
          m_url(url),
          m_finished(false) {
      m_reader = m_handle->obtainReader(this);
    }

    void didGetReadable() override {
      ASSERT(m_reader);
      ASSERT(m_loader);
      ASSERT(m_response);

      WebDataConsumerHandle::Result r = WebDataConsumerHandle::Ok;
      while (r == WebDataConsumerHandle::Ok) {
        const void* buffer;
        size_t size;
        r = m_reader->beginRead(&buffer, WebDataConsumerHandle::FlagNone,
                                &size);
        if (r == WebDataConsumerHandle::Ok) {
          m_buffer.append(static_cast<const char*>(buffer), size);
          m_reader->endRead(size);
        }
      }
      if (r == WebDataConsumerHandle::ShouldWait)
        return;
      String errorMessage =
          "Unknown error occurred while trying to verify integrity.";
      m_finished = true;
      if (r == WebDataConsumerHandle::Done) {
        if (SubresourceIntegrity::CheckSubresourceIntegrity(
                m_integrityMetadata, m_buffer.data(), m_buffer.size(), m_url,
                *m_loader->executionContext(), errorMessage)) {
          m_updater->update(
              new FormDataBytesConsumer(m_buffer.data(), m_buffer.size()));
          m_loader->m_resolver->resolve(m_response);
          m_loader->m_resolver.clear();
          // FetchManager::Loader::didFinishLoading() can
          // be called before didGetReadable() is called
          // when the data is ready. In that case,
          // didFinishLoading() doesn't clean up and call
          // notifyFinished(), so it is necessary to
          // explicitly finish the loader here.
          if (m_loader->m_didFinishLoading)
            m_loader->loadSucceeded();
          return;
        }
      }
      m_updater->update(
          BytesConsumer::createErrored(BytesConsumer::Error(errorMessage)));
      m_loader->performNetworkError(errorMessage);
    }

    bool isFinished() const { return m_finished; }

    DEFINE_INLINE_TRACE() {
      visitor->trace(m_updater);
      visitor->trace(m_response);
      visitor->trace(m_loader);
    }

   private:
    std::unique_ptr<WebDataConsumerHandle> m_handle;
    Member<SRIBytesConsumer> m_updater;
    // We cannot store a Response because its JS wrapper can be collected.
    // TODO(yhirano): Fix this.
    Member<Response> m_response;
    Member<FetchManager::Loader> m_loader;
    String m_integrityMetadata;
    KURL m_url;
    std::unique_ptr<WebDataConsumerHandle::Reader> m_reader;
    Vector<char> m_buffer;
    bool m_finished;
  };

 private:
  Loader(ExecutionContext*,
         FetchManager*,
         ScriptPromiseResolver*,
         FetchRequestData*,
         bool isIsolatedWorld);

  void performBasicFetch();
  void performNetworkError(const String& message);
  void performHTTPFetch(bool corsFlag, bool corsPreflightFlag);
  void performDataFetch();
  void failed(const String& message);
  void notifyFinished();
  Document* document() const;
  ExecutionContext* executionContext() { return m_executionContext; }
  void loadSucceeded();

  Member<FetchManager> m_fetchManager;
  Member<ScriptPromiseResolver> m_resolver;
  Member<FetchRequestData> m_request;
  Member<ThreadableLoader> m_loader;
  bool m_failed;
  bool m_finished;
  int m_responseHttpStatusCode;
  Member<SRIVerifier> m_integrityVerifier;
  bool m_didFinishLoading;
  bool m_isIsolatedWorld;
  Member<ExecutionContext> m_executionContext;
};

FetchManager::Loader::Loader(ExecutionContext* executionContext,
                             FetchManager* fetchManager,
                             ScriptPromiseResolver* resolver,
                             FetchRequestData* request,
                             bool isIsolatedWorld)
    : m_fetchManager(fetchManager),
      m_resolver(resolver),
      m_request(request),
      m_failed(false),
      m_finished(false),
      m_responseHttpStatusCode(0),
      m_integrityVerifier(nullptr),
      m_didFinishLoading(false),
      m_isIsolatedWorld(isIsolatedWorld),
      m_executionContext(executionContext) {
  ThreadState::current()->registerPreFinalizer(this);
}

FetchManager::Loader::~Loader() {
  ASSERT(!m_loader);
}

DEFINE_TRACE(FetchManager::Loader) {
  visitor->trace(m_fetchManager);
  visitor->trace(m_resolver);
  visitor->trace(m_request);
  visitor->trace(m_loader);
  visitor->trace(m_integrityVerifier);
  visitor->trace(m_executionContext);
}

void FetchManager::Loader::didReceiveResponse(
    unsigned long,
    const ResourceResponse& response,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  ASSERT(handle);
  ScriptState* scriptState = m_resolver->getScriptState();
  ScriptState::Scope scope(scriptState);

  if (response.url().protocolIs("blob") && response.httpStatusCode() == 404) {
    // "If |blob| is null, return a network error."
    // https://fetch.spec.whatwg.org/#concept-basic-fetch
    performNetworkError("Blob not found.");
    return;
  }

  if (response.url().protocolIs("blob") && response.httpStatusCode() == 405) {
    performNetworkError("Only 'GET' method is allowed for blob URLs.");
    return;
  }

  m_responseHttpStatusCode = response.httpStatusCode();
  FetchRequestData::Tainting tainting = m_request->responseTainting();

  if (response.url().protocolIsData()) {
    if (m_request->url() == response.url()) {
      // A direct request to data.
      tainting = FetchRequestData::BasicTainting;
    } else {
      // A redirect to data: scheme occured.
      // Redirects to data URLs are rejected by the spec because
      // same-origin data-URL flag is unset, except for no-cors mode.
      // TODO(hiroshige): currently redirects to data URLs in no-cors
      // mode is also rejected by Chromium side.
      switch (m_request->mode()) {
        case WebURLRequest::FetchRequestModeNoCORS:
          tainting = FetchRequestData::OpaqueTainting;
          break;
        case WebURLRequest::FetchRequestModeSameOrigin:
        case WebURLRequest::FetchRequestModeCORS:
        case WebURLRequest::FetchRequestModeCORSWithForcedPreflight:
        case WebURLRequest::FetchRequestModeNavigate:
          performNetworkError("Fetch API cannot load " +
                              m_request->url().getString() +
                              ". Redirects to data: URL are allowed only when "
                              "mode is \"no-cors\".");
          return;
      }
    }
  } else if (!SecurityOrigin::create(response.url())
                  ->isSameSchemeHostPort(m_request->origin().get())) {
    // Recompute the tainting if the request was redirected to a different
    // origin.
    switch (m_request->mode()) {
      case WebURLRequest::FetchRequestModeSameOrigin:
        ASSERT_NOT_REACHED();
        break;
      case WebURLRequest::FetchRequestModeNoCORS:
        tainting = FetchRequestData::OpaqueTainting;
        break;
      case WebURLRequest::FetchRequestModeCORS:
      case WebURLRequest::FetchRequestModeCORSWithForcedPreflight:
        tainting = FetchRequestData::CORSTainting;
        break;
      case WebURLRequest::FetchRequestModeNavigate:
        RELEASE_NOTREACHED();
        break;
    }
  }
  if (response.wasFetchedViaServiceWorker()) {
    switch (response.serviceWorkerResponseType()) {
      case WebServiceWorkerResponseTypeBasic:
      case WebServiceWorkerResponseTypeDefault:
        tainting = FetchRequestData::BasicTainting;
        break;
      case WebServiceWorkerResponseTypeCORS:
        tainting = FetchRequestData::CORSTainting;
        break;
      case WebServiceWorkerResponseTypeOpaque:
        tainting = FetchRequestData::OpaqueTainting;
        break;
      case WebServiceWorkerResponseTypeOpaqueRedirect:
      // ServiceWorker can't respond to the request from fetch() with an
      // opaque redirect response.
      case WebServiceWorkerResponseTypeError:
        // When ServiceWorker respond to the request from fetch() with an
        // error response, FetchManager::Loader::didFail() must be called
        // instead.
        RELEASE_NOTREACHED();
        break;
    }
  }

  FetchResponseData* responseData = nullptr;
  SRIBytesConsumer* sriConsumer = nullptr;
  if (m_request->integrity().isEmpty()) {
    responseData = FetchResponseData::createWithBuffer(new BodyStreamBuffer(
        scriptState,
        new BytesConsumerForDataConsumerHandle(
            scriptState->getExecutionContext(), std::move(handle))));
  } else {
    sriConsumer = new SRIBytesConsumer();
    responseData = FetchResponseData::createWithBuffer(
        new BodyStreamBuffer(scriptState, sriConsumer));
  }
  responseData->setStatus(response.httpStatusCode());
  responseData->setStatusMessage(response.httpStatusText());
  for (auto& it : response.httpHeaderFields())
    responseData->headerList()->append(it.key, it.value);
  responseData->setURL(response.url());
  responseData->setMIMEType(response.mimeType());
  responseData->setResponseTime(response.responseTime());

  FetchResponseData* taintedResponse = nullptr;

  if (NetworkUtils::isRedirectResponseCode(m_responseHttpStatusCode)) {
    Vector<String> locations;
    responseData->headerList()->getAll(HTTPNames::Location, locations);
    if (locations.size() > 1) {
      performNetworkError("Multiple Location header.");
      return;
    }
    if (locations.size() == 1) {
      KURL locationURL(m_request->url(), locations[0]);
      if (!locationURL.isValid()) {
        performNetworkError("Invalid Location header.");
        return;
      }
      ASSERT(m_request->redirect() == WebURLRequest::FetchRedirectModeManual);
      taintedResponse = responseData->createOpaqueRedirectFilteredResponse();
    }
    // When the location header doesn't exist, we don't treat the response
    // as a redirect response, and execute tainting.
  }
  if (!taintedResponse) {
    switch (tainting) {
      case FetchRequestData::BasicTainting:
        taintedResponse = responseData->createBasicFilteredResponse();
        break;
      case FetchRequestData::CORSTainting: {
        HTTPHeaderSet headerNames;
        extractCorsExposedHeaderNamesList(response, headerNames);
        taintedResponse = responseData->createCORSFilteredResponse(headerNames);
        break;
      }
      case FetchRequestData::OpaqueTainting:
        taintedResponse = responseData->createOpaqueFilteredResponse();
        break;
    }
  }

  Response* r =
      Response::create(m_resolver->getExecutionContext(), taintedResponse);
  if (response.url().protocolIsData()) {
    // An "Access-Control-Allow-Origin" header is added for data: URLs
    // but no headers except for "Content-Type" should exist,
    // according to the spec:
    // https://fetch.spec.whatwg.org/#concept-basic-fetch
    // "... return a response whose header list consist of a single header
    //  whose name is `Content-Type` and value is the MIME type and
    //  parameters returned from obtaining a resource"
    r->headers()->headerList()->remove(HTTPNames::Access_Control_Allow_Origin);
  }
  r->headers()->setGuard(Headers::ImmutableGuard);

  if (m_request->integrity().isEmpty()) {
    m_resolver->resolve(r);
    m_resolver.clear();
  } else {
    ASSERT(!m_integrityVerifier);
    m_integrityVerifier =
        new SRIVerifier(std::move(handle), sriConsumer, r, this,
                        m_request->integrity(), response.url());
  }
}

void FetchManager::Loader::didFinishLoading(unsigned long, double) {
  m_didFinishLoading = true;
  // If there is an integrity verifier, and it has not already finished, it
  // will take care of finishing the load or performing a network error when
  // verification is complete.
  if (m_integrityVerifier && !m_integrityVerifier->isFinished())
    return;

  loadSucceeded();
}

void FetchManager::Loader::didFail(const ResourceError& error) {
  if (error.isCancellation() || error.isTimeout() ||
      error.domain() != errorDomainBlinkInternal)
    failed(String());
  else
    failed("Fetch API cannot load " + error.failingURL() + ". " +
           error.localizedDescription());
}

void FetchManager::Loader::didFailAccessControlCheck(
    const ResourceError& error) {
  if (error.isCancellation() || error.isTimeout() ||
      error.domain() != errorDomainBlinkInternal)
    failed(String());
  else
    failed("Fetch API cannot load " + error.failingURL() + ". " +
           error.localizedDescription());
}

void FetchManager::Loader::didFailRedirectCheck() {
  failed("Fetch API cannot load " + m_request->url().getString() +
         ". Redirect failed.");
}

Document* FetchManager::Loader::document() const {
  if (m_executionContext->isDocument()) {
    return toDocument(m_executionContext);
  }
  return nullptr;
}

void FetchManager::Loader::loadSucceeded() {
  ASSERT(!m_failed);

  m_finished = true;

  if (document() && document()->frame() && document()->frame()->page() &&
      m_responseHttpStatusCode >= 200 && m_responseHttpStatusCode < 300) {
    document()->frame()->page()->chromeClient().ajaxSucceeded(
        document()->frame());
  }
  InspectorInstrumentation::didFinishFetch(m_executionContext, this,
                                           m_request->method(),
                                           m_request->url().getString());
  notifyFinished();
}

void FetchManager::Loader::start() {
  // "1. If |request|'s url contains a Known HSTS Host, modify it per the
  // requirements of the 'URI [sic] Loading and Port Mapping' chapter of HTTP
  // Strict Transport Security."
  // FIXME: Implement this.

  // "2. If |request|'s referrer is not none, set |request|'s referrer to the
  // result of invoking determine |request|'s referrer."
  // We set the referrer using workerGlobalScope's URL in
  // WorkerThreadableLoader.

  // "3. If |request|'s synchronous flag is unset and fetch is not invoked
  // recursively, run the remaining steps asynchronously."
  // We don't support synchronous flag.

  // "4. Let response be the value corresponding to the first matching
  // statement:"

  // "- should fetching |request| be blocked as mixed content returns blocked"
  // We do mixed content checking in ResourceFetcher.

  // "- should fetching |request| be blocked as content security returns
  //    blocked"
  if (!ContentSecurityPolicy::shouldBypassMainWorld(m_executionContext) &&
      !m_executionContext->contentSecurityPolicy()->allowConnectToSource(
          m_request->url())) {
    // "A network error."
    performNetworkError(
        "Refused to connect to '" + m_request->url().elidedString() +
        "' because it violates the document's Content Security Policy.");
    return;
  }

  // "- |request|'s url's origin is |request|'s origin and the |CORS flag| is
  //    unset"
  // "- |request|'s url's scheme is 'data' and |request|'s same-origin data
  //    URL flag is set"
  // "- |request|'s url's scheme is 'about'"
  // Note we don't support to call this method with |CORS flag|
  // "- |request|'s mode is |navigate|".
  if ((SecurityOrigin::create(m_request->url())
           ->isSameSchemeHostPortAndSuborigin(m_request->origin().get())) ||
      (m_request->url().protocolIsData() &&
       m_request->sameOriginDataURLFlag()) ||
      (m_request->url().protocolIsAbout()) ||
      (m_request->mode() == WebURLRequest::FetchRequestModeNavigate)) {
    // "The result of performing a basic fetch using request."
    performBasicFetch();
    return;
  }

  // "- |request|'s mode is |same-origin|"
  if (m_request->mode() == WebURLRequest::FetchRequestModeSameOrigin) {
    // "A network error."
    performNetworkError("Fetch API cannot load " +
                        m_request->url().getString() +
                        ". Request mode is \"same-origin\" but the URL\'s "
                        "origin is not same as the request origin " +
                        m_request->origin()->toString() + ".");
    return;
  }

  // "- |request|'s mode is |no CORS|"
  if (m_request->mode() == WebURLRequest::FetchRequestModeNoCORS) {
    // "Set |request|'s response tainting to |opaque|."
    m_request->setResponseTainting(FetchRequestData::OpaqueTainting);
    // "The result of performing a basic fetch using |request|."
    performBasicFetch();
    return;
  }

  // "- |request|'s url's scheme is not one of 'http' and 'https'"
  // This may include other HTTP-like schemes if the embedder has added them
  // to SchemeRegistry::registerURLSchemeAsSupportingFetchAPI.
  if (!SchemeRegistry::shouldTreatURLSchemeAsSupportingFetchAPI(
          m_request->url().protocol())) {
    // "A network error."
    performNetworkError(
        "Fetch API cannot load " + m_request->url().getString() +
        ". URL scheme must be \"http\" or \"https\" for CORS request.");
    return;
  }

  // "- |request|'s mode is |CORS-with-forced-preflight|.
  // "- |request|'s unsafe request flag is set and either |request|'s method
  // is not a simple method or a header in |request|'s header list is not a
  // simple header"
  if (m_request->mode() ==
          WebURLRequest::FetchRequestModeCORSWithForcedPreflight ||
      (m_request->unsafeRequestFlag() &&
       (!FetchUtils::isSimpleMethod(m_request->method()) ||
        m_request->headerList()->containsNonSimpleHeader()))) {
    // "Set |request|'s response tainting to |CORS|."
    m_request->setResponseTainting(FetchRequestData::CORSTainting);
    // "The result of performing an HTTP fetch using |request| with the
    // |CORS flag| and |CORS preflight flag| set."
    performHTTPFetch(true, true);
    return;
  }

  // "- Otherwise
  //     Set |request|'s response tainting to |CORS|."
  m_request->setResponseTainting(FetchRequestData::CORSTainting);
  // "The result of performing an HTTP fetch using |request| with the
  // |CORS flag| set."
  performHTTPFetch(true, false);
}

void FetchManager::Loader::dispose() {
  // Prevent notification
  m_fetchManager = nullptr;
  if (m_loader) {
    m_loader->cancel();
    m_loader = nullptr;
  }
  m_executionContext = nullptr;
}

void FetchManager::Loader::performBasicFetch() {
  // "To perform a basic fetch using |request|, switch on |request|'s url's
  // scheme, and run the associated steps:"
  if (SchemeRegistry::shouldTreatURLSchemeAsSupportingFetchAPI(
          m_request->url().protocol())) {
    // "Return the result of performing an HTTP fetch using |request|."
    performHTTPFetch(false, false);
  } else if (m_request->url().protocolIsData()) {
    performDataFetch();
  } else if (m_request->url().protocolIs("blob")) {
    performHTTPFetch(false, false);
  } else {
    // FIXME: implement other protocols.
    performNetworkError("Fetch API cannot load " +
                        m_request->url().getString() + ". URL scheme \"" +
                        m_request->url().protocol() + "\" is not supported.");
  }
}

void FetchManager::Loader::performNetworkError(const String& message) {
  failed(message);
}

void FetchManager::Loader::performHTTPFetch(bool corsFlag,
                                            bool corsPreflightFlag) {
  ASSERT(
      SchemeRegistry::shouldTreatURLSchemeAsSupportingFetchAPI(
          m_request->url().protocol()) ||
      (m_request->url().protocolIs("blob") && !corsFlag && !corsPreflightFlag));
  // CORS preflight fetch procedure is implemented inside
  // DocumentThreadableLoader.

  // "1. Let |HTTPRequest| be a copy of |request|, except that |HTTPRequest|'s
  //  body is a tee of |request|'s body."
  // We use ResourceRequest class for HTTPRequest.
  // FIXME: Support body.
  ResourceRequest request(m_request->url());
  request.setRequestContext(m_request->context());
  request.setHTTPMethod(m_request->method());
  request.setFetchRequestMode(m_request->mode());
  request.setFetchCredentialsMode(m_request->credentials());
  const Vector<std::unique_ptr<FetchHeaderList::Header>>& list =
      m_request->headerList()->list();
  for (size_t i = 0; i < list.size(); ++i) {
    request.addHTTPHeaderField(AtomicString(list[i]->first),
                               AtomicString(list[i]->second));
  }

  if (m_request->method() != HTTPNames::GET &&
      m_request->method() != HTTPNames::HEAD) {
    if (m_request->buffer())
      request.setHTTPBody(m_request->buffer()->drainAsFormData());
    if (m_request->attachedCredential())
      request.setAttachedCredential(m_request->attachedCredential());
  }
  request.setFetchRedirectMode(m_request->redirect());
  request.setUseStreamOnResponse(true);
  request.setExternalRequestStateFromRequestorAddressSpace(
      m_executionContext->securityContext().addressSpace());

  // "2. Append `Referer`/empty byte sequence, if |HTTPRequest|'s |referrer|
  // is none, and `Referer`/|HTTPRequest|'s referrer, serialized and utf-8
  // encoded, otherwise, to HTTPRequest's header list.
  //
  // The following code also invokes "determine request's referrer" which is
  // written in "Main fetch" operation.
  const ReferrerPolicy referrerPolicy =
      m_request->getReferrerPolicy() == ReferrerPolicyDefault
          ? m_executionContext->getReferrerPolicy()
          : m_request->getReferrerPolicy();
  const String referrerString =
      m_request->referrerString() == FetchRequestData::clientReferrerString()
          ? m_executionContext->outgoingReferrer()
          : m_request->referrerString();
  // Note that generateReferrer generates |no-referrer| from |no-referrer|
  // referrer string (i.e. String()).
  request.setHTTPReferrer(SecurityPolicy::generateReferrer(
      referrerPolicy, m_request->url(), referrerString));
  request.setSkipServiceWorker(m_isIsolatedWorld
                                   ? WebURLRequest::SkipServiceWorker::All
                                   : WebURLRequest::SkipServiceWorker::None);

  // "3. Append `Host`, ..."
  // FIXME: Implement this when the spec is fixed.

  // "4.If |HTTPRequest|'s force Origin header flag is set, append `Origin`/
  // |HTTPRequest|'s origin, serialized and utf-8 encoded, to |HTTPRequest|'s
  // header list."
  // We set Origin header in updateRequestForAccessControl() called from
  // DocumentThreadableLoader::makeCrossOriginAccessRequest

  // "5. Let |credentials flag| be set if either |HTTPRequest|'s credentials
  // mode is |include|, or |HTTPRequest|'s credentials mode is |same-origin|
  // and the |CORS flag| is unset, and unset otherwise.
  //
  // Also, for the last case,
  // https://w3c.github.io/webappsec-suborigins/#security-model-opt-outs:
  // "request's credentials mode is "same-origin" and request's environment
  // settings object has the suborigin unsafe credentials flag set and the
  // request’s current url is same-physical-origin with request origin."
  ResourceLoaderOptions resourceLoaderOptions;
  resourceLoaderOptions.dataBufferingPolicy = DoNotBufferData;
  bool suboriginForcesCredentials =
      (m_request->credentials() ==
           WebURLRequest::FetchCredentialsModeSameOrigin &&
       m_request->origin()->hasSuborigin() &&
       m_request->origin()->suborigin()->policyContains(
           Suborigin::SuboriginPolicyOptions::UnsafeCredentials) &&
       SecurityOrigin::create(m_request->url())
           ->isSameSchemeHostPort(m_request->origin().get()));
  if (m_request->credentials() == WebURLRequest::FetchCredentialsModeInclude ||
      m_request->credentials() == WebURLRequest::FetchCredentialsModePassword ||
      (m_request->credentials() ==
           WebURLRequest::FetchCredentialsModeSameOrigin &&
       !corsFlag) ||
      suboriginForcesCredentials) {
    resourceLoaderOptions.allowCredentials = AllowStoredCredentials;
  }
  if (m_request->credentials() == WebURLRequest::FetchCredentialsModeInclude ||
      m_request->credentials() == WebURLRequest::FetchCredentialsModePassword ||
      suboriginForcesCredentials) {
    resourceLoaderOptions.credentialsRequested = ClientRequestedCredentials;
  }
  resourceLoaderOptions.securityOrigin = m_request->origin().get();

  ThreadableLoaderOptions threadableLoaderOptions;
  threadableLoaderOptions.contentSecurityPolicyEnforcement =
      ContentSecurityPolicy::shouldBypassMainWorld(m_executionContext)
          ? DoNotEnforceContentSecurityPolicy
          : EnforceContentSecurityPolicy;
  if (corsPreflightFlag)
    threadableLoaderOptions.preflightPolicy = ForcePreflight;
  switch (m_request->mode()) {
    case WebURLRequest::FetchRequestModeSameOrigin:
      threadableLoaderOptions.crossOriginRequestPolicy =
          DenyCrossOriginRequests;
      break;
    case WebURLRequest::FetchRequestModeNoCORS:
      threadableLoaderOptions.crossOriginRequestPolicy =
          AllowCrossOriginRequests;
      break;
    case WebURLRequest::FetchRequestModeCORS:
    case WebURLRequest::FetchRequestModeCORSWithForcedPreflight:
      threadableLoaderOptions.crossOriginRequestPolicy = UseAccessControl;
      break;
    case WebURLRequest::FetchRequestModeNavigate:
      // Using DenyCrossOriginRequests here to reduce the security risk.
      // "navigate" request is only available in ServiceWorker.
      threadableLoaderOptions.crossOriginRequestPolicy =
          DenyCrossOriginRequests;
      break;
  }
  InspectorInstrumentation::willStartFetch(m_executionContext, this);
  m_loader =
      ThreadableLoader::create(*m_executionContext, this,
                               threadableLoaderOptions, resourceLoaderOptions);
  m_loader->start(request);
}

// performDataFetch() is almost the same as performHTTPFetch(), except for:
// - We set AllowCrossOriginRequests to allow requests to data: URLs in
//   'same-origin' mode.
// - We reject non-GET method.
void FetchManager::Loader::performDataFetch() {
  ASSERT(m_request->url().protocolIsData());

  // Spec: https://fetch.spec.whatwg.org/#concept-basic-fetch
  // If |request|'s method is `GET` .... Otherwise, return a network error.
  if (m_request->method() != HTTPNames::GET) {
    performNetworkError(
        "Only 'GET' method is allowed for data URLs in Fetch API.");
    return;
  }

  ResourceRequest request(m_request->url());
  request.setRequestContext(m_request->context());
  request.setUseStreamOnResponse(true);
  request.setHTTPMethod(m_request->method());
  request.setFetchRedirectMode(WebURLRequest::FetchRedirectModeError);
  // We intentionally skip 'setExternalRequestStateFromRequestorAddressSpace',
  // as 'data:' can never be external.

  ResourceLoaderOptions resourceLoaderOptions;
  resourceLoaderOptions.dataBufferingPolicy = DoNotBufferData;
  resourceLoaderOptions.securityOrigin = m_request->origin().get();

  ThreadableLoaderOptions threadableLoaderOptions;
  threadableLoaderOptions.contentSecurityPolicyEnforcement =
      ContentSecurityPolicy::shouldBypassMainWorld(m_executionContext)
          ? DoNotEnforceContentSecurityPolicy
          : EnforceContentSecurityPolicy;
  threadableLoaderOptions.crossOriginRequestPolicy = AllowCrossOriginRequests;

  InspectorInstrumentation::willStartFetch(m_executionContext, this);
  m_loader =
      ThreadableLoader::create(*m_executionContext, this,
                               threadableLoaderOptions, resourceLoaderOptions);
  m_loader->start(request);
}

void FetchManager::Loader::failed(const String& message) {
  if (m_failed || m_finished)
    return;
  m_failed = true;
  if (m_executionContext->activeDOMObjectsAreStopped())
    return;
  if (!message.isEmpty())
    m_executionContext->addConsoleMessage(
        ConsoleMessage::create(JSMessageSource, ErrorMessageLevel, message));
  if (m_resolver) {
    ScriptState* state = m_resolver->getScriptState();
    ScriptState::Scope scope(state);
    m_resolver->reject(
        V8ThrowException::createTypeError(state->isolate(), "Failed to fetch"));
  }
  InspectorInstrumentation::didFailFetch(m_executionContext, this);
  notifyFinished();
}

void FetchManager::Loader::notifyFinished() {
  if (m_fetchManager)
    m_fetchManager->onLoaderFinished(this);
}

FetchManager* FetchManager::create(ExecutionContext* executionContext) {
  return new FetchManager(executionContext);
}

FetchManager::FetchManager(ExecutionContext* executionContext)
    : ContextLifecycleObserver(executionContext), m_isStopped(false) {}

ScriptPromise FetchManager::fetch(ScriptState* scriptState,
                                  FetchRequestData* request) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  request->setContext(WebURLRequest::RequestContextFetch);

  Loader* loader =
      Loader::create(getExecutionContext(), this, resolver, request,
                     scriptState->world().isIsolatedWorld());
  m_loaders.add(loader);
  loader->start();
  return promise;
}

void FetchManager::contextDestroyed() {
  ASSERT(!m_isStopped);
  m_isStopped = true;
  for (auto& loader : m_loaders)
    loader->dispose();
}

void FetchManager::onLoaderFinished(Loader* loader) {
  m_loaders.remove(loader);
  loader->dispose();
}

DEFINE_TRACE(FetchManager) {
  visitor->trace(m_loaders);
  ContextLifecycleObserver::trace(visitor);
}

}  // namespace blink
