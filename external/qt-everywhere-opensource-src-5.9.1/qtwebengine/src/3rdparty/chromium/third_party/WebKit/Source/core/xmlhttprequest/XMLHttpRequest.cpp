/*
 *  Copyright (C) 2004, 2006, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2005-2007 Alexey Proskuryakov <ap@webkit.org>
 *  Copyright (C) 2007, 2008 Julien Chaffraix <jchaffraix@webkit.org>
 *  Copyright (C) 2008, 2011 Google Inc. All rights reserved.
 *  Copyright (C) 2012 Intel Corporation
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301 USA
 */

#include "core/xmlhttprequest/XMLHttpRequest.h"

#include "bindings/core/v8/ArrayBufferOrArrayBufferViewOrBlobOrDocumentOrStringOrFormData.h"
#include "bindings/core/v8/ArrayBufferOrArrayBufferViewOrBlobOrUSVString.h"
#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMArrayBufferView.h"
#include "core/dom/DOMException.h"
#include "core/dom/DOMImplementation.h"
#include "core/dom/DOMTypedArray.h"
#include "core/dom/DocumentInit.h"
#include "core/dom/DocumentParser.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/XMLDocument.h"
#include "core/editing/serializers/Serialization.h"
#include "core/events/Event.h"
#include "core/events/ProgressEvent.h"
#include "core/fetch/CrossOriginAccessControl.h"
#include "core/fetch/FetchInitiatorTypeNames.h"
#include "core/fetch/FetchUtils.h"
#include "core/fetch/ResourceLoaderOptions.h"
#include "core/fileapi/Blob.h"
#include "core/fileapi/File.h"
#include "core/fileapi/FileReaderLoader.h"
#include "core/fileapi/FileReaderLoaderClient.h"
#include "core/frame/Deprecation.h"
#include "core/frame/Settings.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/FormData.h"
#include "core/html/HTMLDocument.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/loader/ThreadableLoader.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "core/xmlhttprequest/XMLHttpRequestUpload.h"
#include "platform/FileMetadata.h"
#include "platform/HTTPNames.h"
#include "platform/Histogram.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/SharedBuffer.h"
#include "platform/blob/BlobData.h"
#include "platform/network/HTTPParsers.h"
#include "platform/network/NetworkLog.h"
#include "platform/network/ParsedContentType.h"
#include "platform/network/ResourceError.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityPolicy.h"
#include "platform/weborigin/Suborigin.h"
#include "public/platform/WebURLRequest.h"
#include "wtf/Assertions.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/CString.h"
#include <memory>

namespace blink {

namespace {

// This class protects the wrapper of the associated XMLHttpRequest object
// via hasPendingActivity method which returns true if
// m_eventDispatchRecursionLevel is positive.
class ScopedEventDispatchProtect final {
 public:
  explicit ScopedEventDispatchProtect(int* level) : m_level(level) {
    ++*m_level;
  }
  ~ScopedEventDispatchProtect() {
    DCHECK_GT(*m_level, 0);
    --*m_level;
  }

 private:
  int* const m_level;
};

void replaceCharsetInMediaType(String& mediaType, const String& charsetValue) {
  unsigned pos = 0, len = 0;

  findCharsetInMediaType(mediaType, pos, len);

  if (!len) {
    // When no charset found, do nothing.
    return;
  }

  // Found at least one existing charset, replace all occurrences with new
  // charset.
  while (len) {
    mediaType.replace(pos, len, charsetValue);
    unsigned start = pos + charsetValue.length();
    findCharsetInMediaType(mediaType, pos, len, start);
  }
}

void logConsoleError(ExecutionContext* context, const String& message) {
  if (!context)
    return;
  // FIXME: It's not good to report the bad usage without indicating what source
  // line it came from.  We should pass additional parameters so we can tell the
  // console where the mistake occurred.
  ConsoleMessage* consoleMessage =
      ConsoleMessage::create(JSMessageSource, ErrorMessageLevel, message);
  context->addConsoleMessage(consoleMessage);
}

enum HeaderValueCategoryByRFC7230 {
  HeaderValueInvalid,
  HeaderValueAffectedByNormalization,
  HeaderValueValid,
  HeaderValueCategoryByRFC7230End
};

bool validateOpenArguments(const AtomicString& method,
                           const KURL& url,
                           ExceptionState& exceptionState) {
  if (!isValidHTTPToken(method)) {
    exceptionState.throwDOMException(
        SyntaxError, "'" + method + "' is not a valid HTTP method.");
    return false;
  }

  if (FetchUtils::isForbiddenMethod(method)) {
    exceptionState.throwSecurityError("'" + method +
                                      "' HTTP method is unsupported.");
    return false;
  }

  if (!url.isValid()) {
    exceptionState.throwDOMException(SyntaxError, "Invalid URL");
    return false;
  }

  return true;
}

}  // namespace

class XMLHttpRequest::BlobLoader final
    : public GarbageCollectedFinalized<XMLHttpRequest::BlobLoader>,
      public FileReaderLoaderClient {
 public:
  static BlobLoader* create(XMLHttpRequest* xhr,
                            PassRefPtr<BlobDataHandle> handle) {
    return new BlobLoader(xhr, std::move(handle));
  }

  // FileReaderLoaderClient functions.
  void didStartLoading() override {}
  void didReceiveDataForClient(const char* data, unsigned length) override {
    DCHECK_LE(length, static_cast<unsigned>(INT_MAX));
    m_xhr->didReceiveData(data, length);
  }
  void didFinishLoading() override { m_xhr->didFinishLoadingFromBlob(); }
  void didFail(FileError::ErrorCode error) override {
    m_xhr->didFailLoadingFromBlob();
  }

  void cancel() { m_loader->cancel(); }

  DEFINE_INLINE_TRACE() { visitor->trace(m_xhr); }

 private:
  BlobLoader(XMLHttpRequest* xhr, PassRefPtr<BlobDataHandle> handle)
      : m_xhr(xhr),
        m_loader(
            FileReaderLoader::create(FileReaderLoader::ReadByClient, this)) {
    m_loader->start(m_xhr->getExecutionContext(), std::move(handle));
  }

  Member<XMLHttpRequest> m_xhr;
  std::unique_ptr<FileReaderLoader> m_loader;
};

XMLHttpRequest* XMLHttpRequest::create(ScriptState* scriptState) {
  ExecutionContext* context = scriptState->getExecutionContext();
  DOMWrapperWorld& world = scriptState->world();
  RefPtr<SecurityOrigin> isolatedWorldSecurityOrigin =
      world.isIsolatedWorld() ? world.isolatedWorldSecurityOrigin() : nullptr;
  XMLHttpRequest* xmlHttpRequest =
      new XMLHttpRequest(context, isolatedWorldSecurityOrigin);
  xmlHttpRequest->suspendIfNeeded();

  return xmlHttpRequest;
}

XMLHttpRequest* XMLHttpRequest::create(ExecutionContext* context) {
  XMLHttpRequest* xmlHttpRequest = new XMLHttpRequest(context, nullptr);
  xmlHttpRequest->suspendIfNeeded();

  return xmlHttpRequest;
}

XMLHttpRequest::XMLHttpRequest(
    ExecutionContext* context,
    PassRefPtr<SecurityOrigin> isolatedWorldSecurityOrigin)
    : ActiveScriptWrappable(this),
      ActiveDOMObject(context),
      m_timeoutMilliseconds(0),
      m_responseBlob(this, nullptr),
      m_state(kUnsent),
      m_responseDocument(this, nullptr),
      m_lengthDownloadedToFile(0),
      m_responseArrayBuffer(this, nullptr),
      m_receivedLength(0),
      m_exceptionCode(0),
      m_progressEventThrottle(
          XMLHttpRequestProgressEventThrottle::create(this)),
      m_responseTypeCode(ResponseTypeDefault),
      m_isolatedWorldSecurityOrigin(isolatedWorldSecurityOrigin),
      m_eventDispatchRecursionLevel(0),
      m_async(true),
      m_includeCredentials(false),
      m_parsedResponse(false),
      m_error(false),
      m_uploadEventsAllowed(true),
      m_uploadComplete(false),
      m_sameOriginRequest(true),
      m_downloadingToFile(false),
      m_responseTextOverflow(false),
      m_sendFlag(false) {}

XMLHttpRequest::~XMLHttpRequest() {}

Document* XMLHttpRequest::document() const {
  DCHECK(getExecutionContext()->isDocument());
  return toDocument(getExecutionContext());
}

SecurityOrigin* XMLHttpRequest::getSecurityOrigin() const {
  return m_isolatedWorldSecurityOrigin
             ? m_isolatedWorldSecurityOrigin.get()
             : getExecutionContext()->getSecurityOrigin();
}

XMLHttpRequest::State XMLHttpRequest::readyState() const {
  return m_state;
}

ScriptString XMLHttpRequest::responseText(ExceptionState& exceptionState) {
  if (m_responseTypeCode != ResponseTypeDefault &&
      m_responseTypeCode != ResponseTypeText) {
    exceptionState.throwDOMException(InvalidStateError,
                                     "The value is only accessible if the "
                                     "object's 'responseType' is '' or 'text' "
                                     "(was '" +
                                         responseType() + "').");
    return ScriptString();
  }
  if (m_error || (m_state != kLoading && m_state != kDone))
    return ScriptString();
  return m_responseText;
}

ScriptString XMLHttpRequest::responseJSONSource() {
  DCHECK_EQ(m_responseTypeCode, ResponseTypeJSON);

  if (m_error || m_state != kDone)
    return ScriptString();
  return m_responseText;
}

void XMLHttpRequest::initResponseDocument() {
  // The W3C spec requires the final MIME type to be some valid XML type, or
  // text/html.  If it is text/html, then the responseType of "document" must
  // have been supplied explicitly.
  bool isHTML = responseIsHTML();
  if ((m_response.isHTTP() && !responseIsXML() && !isHTML) ||
      (isHTML && m_responseTypeCode == ResponseTypeDefault) ||
      !getExecutionContext() || getExecutionContext()->isWorkerGlobalScope()) {
    m_responseDocument = nullptr;
    return;
  }

  DocumentInit init =
      DocumentInit::fromContext(document()->contextDocument(), m_url);
  if (isHTML)
    m_responseDocument = HTMLDocument::create(init);
  else
    m_responseDocument = XMLDocument::create(init);

  // FIXME: Set Last-Modified.
  m_responseDocument->setSecurityOrigin(getSecurityOrigin());
  m_responseDocument->setContextFeatures(document()->contextFeatures());
  m_responseDocument->setMimeType(finalResponseMIMETypeWithFallback());
}

Document* XMLHttpRequest::responseXML(ExceptionState& exceptionState) {
  if (m_responseTypeCode != ResponseTypeDefault &&
      m_responseTypeCode != ResponseTypeDocument) {
    exceptionState.throwDOMException(InvalidStateError,
                                     "The value is only accessible if the "
                                     "object's 'responseType' is '' or "
                                     "'document' (was '" +
                                         responseType() + "').");
    return nullptr;
  }

  if (m_error || m_state != kDone)
    return nullptr;

  if (!m_parsedResponse) {
    initResponseDocument();
    if (!m_responseDocument)
      return nullptr;

    m_responseDocument->setContent(m_responseText.flattenToString());
    if (!m_responseDocument->wellFormed())
      m_responseDocument = nullptr;

    m_parsedResponse = true;
  }

  return m_responseDocument.get();
}

Blob* XMLHttpRequest::responseBlob() {
  DCHECK_EQ(m_responseTypeCode, ResponseTypeBlob);

  // We always return null before kDone.
  if (m_error || m_state != kDone)
    return nullptr;

  if (!m_responseBlob) {
    if (m_downloadingToFile) {
      DCHECK(!m_binaryResponseBuilder);

      // When responseType is set to "blob", we redirect the downloaded
      // data to a file-handle directly in the browser process. We get
      // the file-path from the ResourceResponse directly instead of
      // copying the bytes between the browser and the renderer.
      m_responseBlob = Blob::create(createBlobDataHandleFromResponse());
    } else {
      std::unique_ptr<BlobData> blobData = BlobData::create();
      size_t size = 0;
      if (m_binaryResponseBuilder && m_binaryResponseBuilder->size()) {
        size = m_binaryResponseBuilder->size();
        blobData->appendBytes(m_binaryResponseBuilder->data(), size);
        blobData->setContentType(finalResponseMIMETypeWithFallback().lower());
        m_binaryResponseBuilder.clear();
      }
      m_responseBlob =
          Blob::create(BlobDataHandle::create(std::move(blobData), size));
    }
  }

  return m_responseBlob;
}

DOMArrayBuffer* XMLHttpRequest::responseArrayBuffer() {
  DCHECK_EQ(m_responseTypeCode, ResponseTypeArrayBuffer);

  if (m_error || m_state != kDone)
    return nullptr;

  if (!m_responseArrayBuffer) {
    if (m_binaryResponseBuilder && m_binaryResponseBuilder->size()) {
      DOMArrayBuffer* buffer = DOMArrayBuffer::createUninitialized(
          m_binaryResponseBuilder->size(), 1);
      if (!m_binaryResponseBuilder->getAsBytes(
              buffer->data(), static_cast<size_t>(buffer->byteLength()))) {
        // m_binaryResponseBuilder failed to allocate an ArrayBuffer.
        // We need to crash the renderer since there's no way defined in
        // the spec to tell this to the user.
        CRASH();
      }
      m_responseArrayBuffer = buffer;
      m_binaryResponseBuilder.clear();
    } else {
      m_responseArrayBuffer = DOMArrayBuffer::create(nullptr, 0);
    }
  }

  return m_responseArrayBuffer.get();
}

void XMLHttpRequest::setTimeout(unsigned timeout,
                                ExceptionState& exceptionState) {
  // FIXME: Need to trigger or update the timeout Timer here, if needed.
  // http://webkit.org/b/98156
  // XHR2 spec, 4.7.3. "This implies that the timeout attribute can be set while
  // fetching is in progress. If that occurs it will still be measured relative
  // to the start of fetching."
  if (getExecutionContext() && getExecutionContext()->isDocument() &&
      !m_async) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "Timeouts cannot be set for synchronous "
                                     "requests made from a document.");
    return;
  }

  m_timeoutMilliseconds = timeout;

  // From http://www.w3.org/TR/XMLHttpRequest/#the-timeout-attribute:
  // Note: This implies that the timeout attribute can be set while fetching is
  // in progress. If that occurs it will still be measured relative to the start
  // of fetching.
  //
  // The timeout may be overridden after send.
  if (m_loader)
    m_loader->overrideTimeout(timeout);
}

void XMLHttpRequest::setResponseType(const String& responseType,
                                     ExceptionState& exceptionState) {
  if (m_state >= kLoading) {
    exceptionState.throwDOMException(InvalidStateError,
                                     "The response type cannot be set if the "
                                     "object's state is LOADING or DONE.");
    return;
  }

  // Newer functionality is not available to synchronous requests in window
  // contexts, as a spec-mandated attempt to discourage synchronous XHR use.
  // responseType is one such piece of functionality.
  if (getExecutionContext() && getExecutionContext()->isDocument() &&
      !m_async) {
    exceptionState.throwDOMException(InvalidAccessError,
                                     "The response type cannot be changed for "
                                     "synchronous requests made from a "
                                     "document.");
    return;
  }

  if (responseType == "") {
    m_responseTypeCode = ResponseTypeDefault;
  } else if (responseType == "text") {
    m_responseTypeCode = ResponseTypeText;
  } else if (responseType == "json") {
    m_responseTypeCode = ResponseTypeJSON;
  } else if (responseType == "document") {
    m_responseTypeCode = ResponseTypeDocument;
  } else if (responseType == "blob") {
    m_responseTypeCode = ResponseTypeBlob;
  } else if (responseType == "arraybuffer") {
    m_responseTypeCode = ResponseTypeArrayBuffer;
  } else {
    NOTREACHED();
  }
}

String XMLHttpRequest::responseType() {
  switch (m_responseTypeCode) {
    case ResponseTypeDefault:
      return "";
    case ResponseTypeText:
      return "text";
    case ResponseTypeJSON:
      return "json";
    case ResponseTypeDocument:
      return "document";
    case ResponseTypeBlob:
      return "blob";
    case ResponseTypeArrayBuffer:
      return "arraybuffer";
  }
  return "";
}

String XMLHttpRequest::responseURL() {
  KURL responseURL(m_response.url());
  if (!responseURL.isNull())
    responseURL.removeFragmentIdentifier();
  return responseURL.getString();
}

XMLHttpRequestUpload* XMLHttpRequest::upload() {
  if (!m_upload)
    m_upload = XMLHttpRequestUpload::create(this);
  return m_upload.get();
}

void XMLHttpRequest::trackProgress(long long length) {
  m_receivedLength += length;

  changeState(kLoading);
  if (m_async) {
    // readyStateChange event is fired as well.
    dispatchProgressEventFromSnapshot(EventTypeNames::progress);
  }
}

void XMLHttpRequest::changeState(State newState) {
  if (m_state != newState) {
    m_state = newState;
    dispatchReadyStateChangeEvent();
  }
}

void XMLHttpRequest::dispatchReadyStateChangeEvent() {
  if (!getExecutionContext())
    return;

  ScopedEventDispatchProtect protect(&m_eventDispatchRecursionLevel);
  if (m_async || (m_state <= kOpened || m_state == kDone)) {
    TRACE_EVENT1(
        "devtools.timeline", "XHRReadyStateChange", "data",
        InspectorXhrReadyStateChangeEvent::data(getExecutionContext(), this));
    XMLHttpRequestProgressEventThrottle::DeferredEventAction action =
        XMLHttpRequestProgressEventThrottle::Ignore;
    if (m_state == kDone) {
      if (m_error)
        action = XMLHttpRequestProgressEventThrottle::Clear;
      else
        action = XMLHttpRequestProgressEventThrottle::Flush;
    }
    m_progressEventThrottle->dispatchReadyStateChangeEvent(
        Event::create(EventTypeNames::readystatechange), action);
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                         "UpdateCounters", TRACE_EVENT_SCOPE_THREAD, "data",
                         InspectorUpdateCountersEvent::data());
  }

  if (m_state == kDone && !m_error) {
    TRACE_EVENT1("devtools.timeline", "XHRLoad", "data",
                 InspectorXhrLoadEvent::data(getExecutionContext(), this));
    dispatchProgressEventFromSnapshot(EventTypeNames::load);
    dispatchProgressEventFromSnapshot(EventTypeNames::loadend);
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                         "UpdateCounters", TRACE_EVENT_SCOPE_THREAD, "data",
                         InspectorUpdateCountersEvent::data());
  }
}

void XMLHttpRequest::setWithCredentials(bool value,
                                        ExceptionState& exceptionState) {
  if (m_state > kOpened || m_sendFlag) {
    exceptionState.throwDOMException(
        InvalidStateError,
        "The value may only be set if the object's state is UNSENT or OPENED.");
    return;
  }

  m_includeCredentials = value;
}

void XMLHttpRequest::open(const AtomicString& method,
                          const String& urlString,
                          ExceptionState& exceptionState) {
  if (!getExecutionContext())
    return;

  KURL url(getExecutionContext()->completeURL(urlString));
  if (!validateOpenArguments(method, url, exceptionState))
    return;

  open(method, url, true, exceptionState);
}

void XMLHttpRequest::open(const AtomicString& method,
                          const String& urlString,
                          bool async,
                          const String& username,
                          const String& password,
                          ExceptionState& exceptionState) {
  if (!getExecutionContext())
    return;

  KURL url(getExecutionContext()->completeURL(urlString));
  if (!validateOpenArguments(method, url, exceptionState))
    return;

  if (!username.isNull())
    url.setUser(username);
  if (!password.isNull())
    url.setPass(password);

  open(method, url, async, exceptionState);
}

void XMLHttpRequest::open(const AtomicString& method,
                          const KURL& url,
                          bool async,
                          ExceptionState& exceptionState) {
  NETWORK_DVLOG(1) << this << " open(" << method << ", " << url.elidedString()
                   << ", " << async << ")";

  DCHECK(validateOpenArguments(method, url, exceptionState));

  if (!internalAbort())
    return;

  State previousState = m_state;
  m_state = kUnsent;
  m_error = false;
  m_uploadComplete = false;

  if (!ContentSecurityPolicy::shouldBypassMainWorld(getExecutionContext()) &&
      !getExecutionContext()->contentSecurityPolicy()->allowConnectToSource(
          url)) {
    // We can safely expose the URL to JavaScript, as these checks happen
    // synchronously before redirection. JavaScript receives no new information.
    exceptionState.throwSecurityError(
        "Refused to connect to '" + url.elidedString() +
        "' because it violates the document's Content Security Policy.");
    return;
  }

  if (!async && getExecutionContext()->isDocument()) {
    if (document()->settings() &&
        !document()->settings()->syncXHRInDocumentsEnabled()) {
      exceptionState.throwDOMException(
          InvalidAccessError,
          "Synchronous requests are disabled for this page.");
      return;
    }

    // Newer functionality is not available to synchronous requests in window
    // contexts, as a spec-mandated attempt to discourage synchronous XHR use.
    // responseType is one such piece of functionality.
    if (m_responseTypeCode != ResponseTypeDefault) {
      exceptionState.throwDOMException(
          InvalidAccessError,
          "Synchronous requests from a document must not set a response type.");
      return;
    }

    // Similarly, timeouts are disabled for synchronous requests as well.
    if (m_timeoutMilliseconds > 0) {
      exceptionState.throwDOMException(
          InvalidAccessError, "Synchronous requests must not set a timeout.");
      return;
    }

    // Here we just warn that firing sync XHR's may affect responsiveness.
    // Eventually sync xhr will be deprecated and an "InvalidAccessError"
    // exception thrown.
    // Refer : https://xhr.spec.whatwg.org/#sync-warning
    // Use count for XHR synchronous requests on main thread only.
    if (!document()->processingBeforeUnload())
      Deprecation::countDeprecation(
          getExecutionContext(),
          UseCounter::XMLHttpRequestSynchronousInNonWorkerOutsideBeforeUnload);
  }

  m_method = FetchUtils::normalizeMethod(method);

  m_url = url;

  m_async = async;

  DCHECK(!m_loader);
  m_sendFlag = false;

  // Check previous state to avoid dispatching readyState event
  // when calling open several times in a row.
  if (previousState != kOpened)
    changeState(kOpened);
  else
    m_state = kOpened;
}

bool XMLHttpRequest::initSend(ExceptionState& exceptionState) {
  if (!getExecutionContext()) {
    handleNetworkError();
    throwForLoadFailureIfNeeded(exceptionState,
                                "Document is already detached.");
    return false;
  }

  if (m_state != kOpened || m_sendFlag) {
    exceptionState.throwDOMException(InvalidStateError,
                                     "The object's state must be OPENED.");
    return false;
  }

  if (!m_async) {
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    if (isolate && v8::MicrotasksScope::IsRunningMicrotasks(isolate)) {
      UseCounter::count(getExecutionContext(),
                        UseCounter::During_Microtask_SyncXHR);
    }
  }

  m_error = false;
  return true;
}

void XMLHttpRequest::send(
    const ArrayBufferOrArrayBufferViewOrBlobOrDocumentOrStringOrFormData& body,
    ExceptionState& exceptionState) {
  InspectorInstrumentation::willSendXMLHttpOrFetchNetworkRequest(
      getExecutionContext(), url());

  if (body.isNull()) {
    send(String(), exceptionState);
    return;
  }

  if (body.isArrayBuffer()) {
    send(body.getAsArrayBuffer(), exceptionState);
    return;
  }

  if (body.isArrayBufferView()) {
    send(body.getAsArrayBufferView(), exceptionState);
    return;
  }

  if (body.isBlob()) {
    send(body.getAsBlob(), exceptionState);
    return;
  }

  if (body.isDocument()) {
    send(body.getAsDocument(), exceptionState);
    return;
  }

  if (body.isFormData()) {
    send(body.getAsFormData(), exceptionState);
    return;
  }

  DCHECK(body.isString());
  send(body.getAsString(), exceptionState);
}

bool XMLHttpRequest::areMethodAndURLValidForSend() {
  return m_method != HTTPNames::GET && m_method != HTTPNames::HEAD &&
         m_url.protocolIsInHTTPFamily();
}

void XMLHttpRequest::send(Document* document, ExceptionState& exceptionState) {
  NETWORK_DVLOG(1) << this << " send() Document "
                   << static_cast<void*>(document);

  DCHECK(document);

  if (!initSend(exceptionState))
    return;

  RefPtr<EncodedFormData> httpBody;

  if (areMethodAndURLValidForSend()) {
    // FIXME: Per https://xhr.spec.whatwg.org/#dom-xmlhttprequest-send the
    // Content-Type header and whether to serialize as HTML or XML should
    // depend on |document->isHTMLDocument()|.
    if (getRequestHeader(HTTPNames::Content_Type).isEmpty())
      setRequestHeaderInternal(HTTPNames::Content_Type,
                               "application/xml;charset=UTF-8");

    String body = createMarkup(document);

    httpBody = EncodedFormData::create(
        UTF8Encoding().encode(body, WTF::EntitiesForUnencodables));
  }

  createRequest(httpBody.release(), exceptionState);
}

void XMLHttpRequest::send(const String& body, ExceptionState& exceptionState) {
  NETWORK_DVLOG(1) << this << " send() String " << body;

  if (!initSend(exceptionState))
    return;

  RefPtr<EncodedFormData> httpBody;

  if (!body.isNull() && areMethodAndURLValidForSend()) {
    String contentType = getRequestHeader(HTTPNames::Content_Type);
    if (contentType.isEmpty()) {
      setRequestHeaderInternal(HTTPNames::Content_Type,
                               "text/plain;charset=UTF-8");
    } else {
      replaceCharsetInMediaType(contentType, "UTF-8");
      m_requestHeaders.set(HTTPNames::Content_Type, AtomicString(contentType));
    }

    httpBody = EncodedFormData::create(
        UTF8Encoding().encode(body, WTF::EntitiesForUnencodables));
  }

  createRequest(httpBody.release(), exceptionState);
}

void XMLHttpRequest::send(Blob* body, ExceptionState& exceptionState) {
  NETWORK_DVLOG(1) << this << " send() Blob " << body->uuid();

  if (!initSend(exceptionState))
    return;

  RefPtr<EncodedFormData> httpBody;

  if (areMethodAndURLValidForSend()) {
    if (getRequestHeader(HTTPNames::Content_Type).isEmpty()) {
      const String& blobType = body->type();
      if (!blobType.isEmpty() && isValidContentType(blobType)) {
        setRequestHeaderInternal(HTTPNames::Content_Type,
                                 AtomicString(blobType));
      }
    }

    // FIXME: add support for uploading bundles.
    httpBody = EncodedFormData::create();
    if (body->hasBackingFile()) {
      File* file = toFile(body);
      if (!file->path().isEmpty())
        httpBody->appendFile(file->path());
      else if (!file->fileSystemURL().isEmpty())
        httpBody->appendFileSystemURL(file->fileSystemURL());
      else
        NOTREACHED();
    } else {
      httpBody->appendBlob(body->uuid(), body->blobDataHandle());
    }
  }

  createRequest(httpBody.release(), exceptionState);
}

void XMLHttpRequest::send(FormData* body, ExceptionState& exceptionState) {
  NETWORK_DVLOG(1) << this << " send() FormData " << body;

  if (!initSend(exceptionState))
    return;

  RefPtr<EncodedFormData> httpBody;

  if (areMethodAndURLValidForSend()) {
    httpBody = body->encodeMultiPartFormData();

    if (getRequestHeader(HTTPNames::Content_Type).isEmpty()) {
      AtomicString contentType =
          AtomicString("multipart/form-data; boundary=") +
          httpBody->boundary().data();
      setRequestHeaderInternal(HTTPNames::Content_Type, contentType);
    }
  }

  createRequest(httpBody.release(), exceptionState);
}

void XMLHttpRequest::send(DOMArrayBuffer* body,
                          ExceptionState& exceptionState) {
  NETWORK_DVLOG(1) << this << " send() ArrayBuffer " << body;

  sendBytesData(body->data(), body->byteLength(), exceptionState);
}

void XMLHttpRequest::send(DOMArrayBufferView* body,
                          ExceptionState& exceptionState) {
  NETWORK_DVLOG(1) << this << " send() ArrayBufferView " << body;

  sendBytesData(body->baseAddress(), body->byteLength(), exceptionState);
}

void XMLHttpRequest::sendBytesData(const void* data,
                                   size_t length,
                                   ExceptionState& exceptionState) {
  if (!initSend(exceptionState))
    return;

  RefPtr<EncodedFormData> httpBody;

  if (areMethodAndURLValidForSend()) {
    httpBody = EncodedFormData::create(data, length);
  }

  createRequest(httpBody.release(), exceptionState);
}

void XMLHttpRequest::sendForInspectorXHRReplay(
    PassRefPtr<EncodedFormData> formData,
    ExceptionState& exceptionState) {
  createRequest(formData ? formData->deepCopy() : nullptr, exceptionState);
  m_exceptionCode = exceptionState.code();
}

void XMLHttpRequest::throwForLoadFailureIfNeeded(ExceptionState& exceptionState,
                                                 const String& reason) {
  if (m_error && !m_exceptionCode)
    m_exceptionCode = NetworkError;

  if (!m_exceptionCode)
    return;

  String message = "Failed to load '" + m_url.elidedString() + "'";
  if (reason.isNull()) {
    message.append('.');
  } else {
    message.append(": ");
    message.append(reason);
  }

  exceptionState.throwDOMException(m_exceptionCode, message);
}

void XMLHttpRequest::createRequest(PassRefPtr<EncodedFormData> httpBody,
                                   ExceptionState& exceptionState) {
  // Only GET request is supported for blob URL.
  if (m_url.protocolIs("blob") && m_method != HTTPNames::GET) {
    handleNetworkError();

    if (!m_async) {
      throwForLoadFailureIfNeeded(
          exceptionState, "'GET' is the only method allowed for 'blob:' URLs.");
    }
    return;
  }

  DCHECK(getExecutionContext());
  ExecutionContext& executionContext = *getExecutionContext();

  m_sendFlag = true;
  // The presence of upload event listeners forces us to use preflighting
  // because POSTing to an URL that does not permit cross origin requests should
  // look exactly like POSTing to an URL that does not respond at all.
  // Also, only async requests support upload progress events.
  bool uploadEvents = false;
  if (m_async) {
    InspectorInstrumentation::asyncTaskScheduled(
        &executionContext, "XMLHttpRequest.send", this, true);
    dispatchProgressEvent(EventTypeNames::loadstart, 0, 0);
    if (httpBody && m_upload) {
      uploadEvents = m_upload->hasEventListeners();
      m_upload->dispatchEvent(
          ProgressEvent::create(EventTypeNames::loadstart, false, 0, 0));
    }
  }

  m_sameOriginRequest = getSecurityOrigin()->canRequestNoSuborigin(m_url);

  // Per https://w3c.github.io/webappsec-suborigins/#security-model-opt-outs,
  // credentials are forced when credentials mode is "same-origin", the
  // 'unsafe-credentials' option is set, and the request's physical origin is
  // the same as the URL's.
  bool includeCredentials =
      m_includeCredentials ||
      (getSecurityOrigin()->hasSuborigin() &&
       getSecurityOrigin()->suborigin()->policyContains(
           Suborigin::SuboriginPolicyOptions::UnsafeCredentials) &&
       SecurityOrigin::create(m_url)->isSameSchemeHostPort(
           getSecurityOrigin()));

  if (!m_sameOriginRequest && includeCredentials)
    UseCounter::count(&executionContext,
                      UseCounter::XMLHttpRequestCrossOriginWithCredentials);

  // We also remember whether upload events should be allowed for this request
  // in case the upload listeners are added after the request is started.
  m_uploadEventsAllowed =
      m_sameOriginRequest || uploadEvents ||
      !FetchUtils::isSimpleRequest(m_method, m_requestHeaders);

  ResourceRequest request(m_url);
  request.setHTTPMethod(m_method);
  request.setRequestContext(WebURLRequest::RequestContextXMLHttpRequest);
  request.setFetchCredentialsMode(
      includeCredentials ? WebURLRequest::FetchCredentialsModeInclude
                         : WebURLRequest::FetchCredentialsModeSameOrigin);
  request.setSkipServiceWorker(m_isolatedWorldSecurityOrigin.get()
                                   ? WebURLRequest::SkipServiceWorker::All
                                   : WebURLRequest::SkipServiceWorker::None);
  request.setExternalRequestStateFromRequestorAddressSpace(
      executionContext.securityContext().addressSpace());

  InspectorInstrumentation::willLoadXHR(
      &executionContext, this, this, m_method, m_url, m_async,
      httpBody ? httpBody->deepCopy() : nullptr, m_requestHeaders,
      includeCredentials);

  if (httpBody) {
    DCHECK_NE(m_method, HTTPNames::GET);
    DCHECK_NE(m_method, HTTPNames::HEAD);
    request.setHTTPBody(std::move(httpBody));
  }

  if (m_requestHeaders.size() > 0)
    request.addHTTPHeaderFields(m_requestHeaders);

  ThreadableLoaderOptions options;
  options.preflightPolicy = uploadEvents ? ForcePreflight : ConsiderPreflight;
  options.crossOriginRequestPolicy = UseAccessControl;
  options.initiator = FetchInitiatorTypeNames::xmlhttprequest;
  options.contentSecurityPolicyEnforcement =
      ContentSecurityPolicy::shouldBypassMainWorld(&executionContext)
          ? DoNotEnforceContentSecurityPolicy
          : EnforceContentSecurityPolicy;
  options.timeoutMilliseconds = m_timeoutMilliseconds;

  ResourceLoaderOptions resourceLoaderOptions;
  resourceLoaderOptions.allowCredentials =
      (m_sameOriginRequest || includeCredentials) ? AllowStoredCredentials
                                                  : DoNotAllowStoredCredentials;
  resourceLoaderOptions.credentialsRequested =
      includeCredentials ? ClientRequestedCredentials
                         : ClientDidNotRequestCredentials;
  resourceLoaderOptions.securityOrigin = getSecurityOrigin();

  // When responseType is set to "blob", we redirect the downloaded data to a
  // file-handle directly.
  m_downloadingToFile = getResponseTypeCode() == ResponseTypeBlob;
  if (m_downloadingToFile) {
    request.setDownloadToFile(true);
    resourceLoaderOptions.dataBufferingPolicy = DoNotBufferData;
  }

  if (m_async) {
    resourceLoaderOptions.dataBufferingPolicy = DoNotBufferData;
  }

  m_exceptionCode = 0;
  m_error = false;

  if (m_async) {
    UseCounter::count(&executionContext,
                      UseCounter::XMLHttpRequestAsynchronous);
    if (m_upload)
      request.setReportUploadProgress(true);

    DCHECK(!m_loader);
    DCHECK(m_sendFlag);
    m_loader = ThreadableLoader::create(executionContext, this, options,
                                        resourceLoaderOptions);
    m_loader->start(request);

    return;
  }

  // Use count for XHR synchronous requests.
  UseCounter::count(&executionContext, UseCounter::XMLHttpRequestSynchronous);
  ThreadableLoader::loadResourceSynchronously(executionContext, request, *this,
                                              options, resourceLoaderOptions);

  throwForLoadFailureIfNeeded(exceptionState, String());
}

void XMLHttpRequest::abort() {
  NETWORK_DVLOG(1) << this << " abort()";

  // internalAbort() clears the response. Save the data needed for
  // dispatching ProgressEvents.
  long long expectedLength = m_response.expectedContentLength();
  long long receivedLength = m_receivedLength;

  if (!internalAbort())
    return;

  // The script never gets any chance to call abort() on a sync XHR between
  // send() call and transition to the DONE state. It's because a sync XHR
  // doesn't dispatch any event between them. So, if |m_async| is false, we
  // can skip the "request error steps" (defined in the XHR spec) without any
  // state check.
  //
  // FIXME: It's possible open() is invoked in internalAbort() and |m_async|
  // becomes true by that. We should implement more reliable treatment for
  // nested method invocations at some point.
  if (m_async) {
    if ((m_state == kOpened && m_sendFlag) || m_state == kHeadersReceived ||
        m_state == kLoading) {
      DCHECK(!m_loader);
      handleRequestError(0, EventTypeNames::abort, receivedLength,
                         expectedLength);
    }
  }
  m_state = kUnsent;
}

void XMLHttpRequest::dispose() {
  if (m_loader) {
    m_error = true;
    m_loader->cancel();
  }
}

void XMLHttpRequest::clearVariablesForLoading() {
  if (m_blobLoader) {
    m_blobLoader->cancel();
    m_blobLoader = nullptr;
  }

  m_decoder.reset();

  if (m_responseDocumentParser) {
    m_responseDocumentParser->removeClient(this);
    m_responseDocumentParser->detach();
    m_responseDocumentParser = nullptr;
  }

  m_finalResponseCharset = String();
}

bool XMLHttpRequest::internalAbort() {
  m_error = true;

  if (m_responseDocumentParser && !m_responseDocumentParser->isStopped())
    m_responseDocumentParser->stopParsing();

  clearVariablesForLoading();

  clearResponse();
  clearRequest();

  if (!m_loader)
    return true;

  // Cancelling the ThreadableLoader m_loader may result in calling
  // window.onload synchronously. If such an onload handler contains open()
  // call on the same XMLHttpRequest object, reentry happens.
  //
  // If, window.onload contains open() and send(), m_loader will be set to
  // non 0 value. So, we cannot continue the outer open(). In such case,
  // just abort the outer open() by returning false.
  ThreadableLoader* loader = m_loader.release();
  loader->cancel();

  // If abort() called internalAbort() and a nested open() ended up
  // clearing the error flag, but didn't send(), make sure the error
  // flag is still set.
  bool newLoadStarted = m_loader.get();
  if (!newLoadStarted)
    m_error = true;

  return !newLoadStarted;
}

void XMLHttpRequest::clearResponse() {
  // FIXME: when we add the support for multi-part XHR, we will have to
  // be careful with this initialization.
  m_receivedLength = 0;

  m_response = ResourceResponse();

  m_responseText.clear();

  m_parsedResponse = false;
  m_responseDocument = nullptr;

  m_responseBlob = nullptr;

  m_downloadingToFile = false;
  m_lengthDownloadedToFile = 0;

  // These variables may referred by the response accessors. So, we can clear
  // this only when we clear the response holder variables above.
  m_binaryResponseBuilder.clear();
  m_responseArrayBuffer.clear();
}

void XMLHttpRequest::clearRequest() {
  m_requestHeaders.clear();
}

void XMLHttpRequest::dispatchProgressEvent(const AtomicString& type,
                                           long long receivedLength,
                                           long long expectedLength) {
  bool lengthComputable =
      expectedLength > 0 && receivedLength <= expectedLength;
  unsigned long long loaded =
      receivedLength >= 0 ? static_cast<unsigned long long>(receivedLength) : 0;
  unsigned long long total =
      lengthComputable ? static_cast<unsigned long long>(expectedLength) : 0;

  ExecutionContext* context = getExecutionContext();
  InspectorInstrumentation::AsyncTask asyncTask(context, this, m_async);
  m_progressEventThrottle->dispatchProgressEvent(type, lengthComputable, loaded,
                                                 total);
  if (m_async && type == EventTypeNames::loadend)
    InspectorInstrumentation::asyncTaskCanceled(context, this);
}

void XMLHttpRequest::dispatchProgressEventFromSnapshot(
    const AtomicString& type) {
  dispatchProgressEvent(type, m_receivedLength,
                        m_response.expectedContentLength());
}

void XMLHttpRequest::handleNetworkError() {
  NETWORK_DVLOG(1) << this << " handleNetworkError()";

  // Response is cleared next, save needed progress event data.
  long long expectedLength = m_response.expectedContentLength();
  long long receivedLength = m_receivedLength;

  if (!internalAbort())
    return;

  handleRequestError(NetworkError, EventTypeNames::error, receivedLength,
                     expectedLength);
}

void XMLHttpRequest::handleDidCancel() {
  NETWORK_DVLOG(1) << this << " handleDidCancel()";

  // Response is cleared next, save needed progress event data.
  long long expectedLength = m_response.expectedContentLength();
  long long receivedLength = m_receivedLength;

  if (!internalAbort())
    return;

  handleRequestError(AbortError, EventTypeNames::abort, receivedLength,
                     expectedLength);
}

void XMLHttpRequest::handleRequestError(ExceptionCode exceptionCode,
                                        const AtomicString& type,
                                        long long receivedLength,
                                        long long expectedLength) {
  NETWORK_DVLOG(1) << this << " handleRequestError()";

  InspectorInstrumentation::didFailXHRLoading(getExecutionContext(), this, this,
                                              m_method, m_url);

  m_sendFlag = false;
  if (!m_async) {
    DCHECK(exceptionCode);
    m_state = kDone;
    m_exceptionCode = exceptionCode;
    return;
  }

  // With m_error set, the state change steps are minimal: any pending
  // progress event is flushed + a readystatechange is dispatched.
  // No new progress events dispatched; as required, that happens at
  // the end here.
  DCHECK(m_error);
  changeState(kDone);

  if (!m_uploadComplete) {
    m_uploadComplete = true;
    if (m_upload && m_uploadEventsAllowed)
      m_upload->handleRequestError(type);
  }

  // Note: The below event dispatch may be called while |hasPendingActivity() ==
  // false|, when |handleRequestError| is called after |internalAbort()|.  This
  // is safe, however, as |this| will be kept alive from a strong ref
  // |Event::m_target|.
  dispatchProgressEvent(EventTypeNames::progress, receivedLength,
                        expectedLength);
  dispatchProgressEvent(type, receivedLength, expectedLength);
  dispatchProgressEvent(EventTypeNames::loadend, receivedLength,
                        expectedLength);
}

void XMLHttpRequest::overrideMimeType(const AtomicString& mimeType,
                                      ExceptionState& exceptionState) {
  if (m_state == kLoading || m_state == kDone) {
    exceptionState.throwDOMException(
        InvalidStateError,
        "MimeType cannot be overridden when the state is LOADING or DONE.");
    return;
  }

  m_mimeTypeOverride = mimeType;
}

void XMLHttpRequest::setRequestHeader(const AtomicString& name,
                                      const AtomicString& value,
                                      ExceptionState& exceptionState) {
  if (m_state != kOpened || m_sendFlag) {
    exceptionState.throwDOMException(InvalidStateError,
                                     "The object's state must be OPENED.");
    return;
  }

  if (!isValidHTTPToken(name)) {
    exceptionState.throwDOMException(
        SyntaxError, "'" + name + "' is not a valid HTTP header field name.");
    return;
  }

  if (!isValidHTTPHeaderValue(value)) {
    exceptionState.throwDOMException(
        SyntaxError, "'" + value + "' is not a valid HTTP header field value.");
    return;
  }

  // No script (privileged or not) can set unsafe headers.
  if (FetchUtils::isForbiddenHeaderName(name)) {
    logConsoleError(getExecutionContext(),
                    "Refused to set unsafe header \"" + name + "\"");
    return;
  }

  setRequestHeaderInternal(name, value);
}

void XMLHttpRequest::setRequestHeaderInternal(const AtomicString& name,
                                              const AtomicString& value) {
  HeaderValueCategoryByRFC7230 headerValueCategory = HeaderValueValid;

  HTTPHeaderMap::AddResult result = m_requestHeaders.add(name, value);
  if (!result.isNewEntry) {
    AtomicString newValue = result.storedValue->value + ", " + value;

    // Without normalization at XHR level here, the actual header value
    // sent to the network is |newValue| with leading/trailing whitespaces
    // stripped (i.e. |normalizeHeaderValue(newValue)|).
    // With normalization at XHR level here as the spec requires, the
    // actual header value sent to the network is |normalizedNewValue|.
    // If these two are different, introducing normalization here affects
    // the header value sent to the network.
    String normalizedNewValue =
        FetchUtils::normalizeHeaderValue(result.storedValue->value) + ", " +
        FetchUtils::normalizeHeaderValue(value);
    if (FetchUtils::normalizeHeaderValue(newValue) != normalizedNewValue)
      headerValueCategory = HeaderValueAffectedByNormalization;

    result.storedValue->value = newValue;
  }

  String normalizedValue = FetchUtils::normalizeHeaderValue(value);
  if (!normalizedValue.isEmpty() &&
      !isValidHTTPFieldContentRFC7230(normalizedValue))
    headerValueCategory = HeaderValueInvalid;

  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, headerValueCategoryHistogram,
      new EnumerationHistogram(
          "Blink.XHR.setRequestHeader.HeaderValueCategoryInRFC7230",
          HeaderValueCategoryByRFC7230End));
  headerValueCategoryHistogram.count(headerValueCategory);
}

const AtomicString& XMLHttpRequest::getRequestHeader(
    const AtomicString& name) const {
  return m_requestHeaders.get(name);
}

String XMLHttpRequest::getAllResponseHeaders() const {
  if (m_state < kHeadersReceived || m_error)
    return "";

  StringBuilder stringBuilder;

  HTTPHeaderSet accessControlExposeHeaderSet;
  extractCorsExposedHeaderNamesList(m_response, accessControlExposeHeaderSet);

  HTTPHeaderMap::const_iterator end = m_response.httpHeaderFields().end();
  for (HTTPHeaderMap::const_iterator it = m_response.httpHeaderFields().begin();
       it != end; ++it) {
    // Hide any headers whose name is a forbidden response-header name.
    // This is required for all kinds of filtered responses.
    //
    // TODO: Consider removing canLoadLocalResources() call.
    // crbug.com/567527
    if (FetchUtils::isForbiddenResponseHeaderName(it->key) &&
        !getSecurityOrigin()->canLoadLocalResources())
      continue;

    if (!m_sameOriginRequest &&
        !isOnAccessControlResponseHeaderWhitelist(it->key) &&
        !accessControlExposeHeaderSet.contains(it->key))
      continue;

    stringBuilder.append(it->key);
    stringBuilder.append(':');
    stringBuilder.append(' ');
    stringBuilder.append(it->value);
    stringBuilder.append('\r');
    stringBuilder.append('\n');
  }

  return stringBuilder.toString();
}

const AtomicString& XMLHttpRequest::getResponseHeader(
    const AtomicString& name) const {
  if (m_state < kHeadersReceived || m_error)
    return nullAtom;

  // See comment in getAllResponseHeaders above.
  if (FetchUtils::isForbiddenResponseHeaderName(name) &&
      !getSecurityOrigin()->canLoadLocalResources()) {
    logConsoleError(getExecutionContext(),
                    "Refused to get unsafe header \"" + name + "\"");
    return nullAtom;
  }

  HTTPHeaderSet accessControlExposeHeaderSet;
  extractCorsExposedHeaderNamesList(m_response, accessControlExposeHeaderSet);

  if (!m_sameOriginRequest && !isOnAccessControlResponseHeaderWhitelist(name) &&
      !accessControlExposeHeaderSet.contains(name)) {
    logConsoleError(getExecutionContext(),
                    "Refused to get unsafe header \"" + name + "\"");
    return nullAtom;
  }
  return m_response.httpHeaderField(name);
}

AtomicString XMLHttpRequest::finalResponseMIMEType() const {
  AtomicString overriddenType =
      extractMIMETypeFromMediaType(m_mimeTypeOverride);
  if (!overriddenType.isEmpty())
    return overriddenType;

  if (m_response.isHTTP())
    return extractMIMETypeFromMediaType(
        m_response.httpHeaderField(HTTPNames::Content_Type));

  return m_response.mimeType();
}

AtomicString XMLHttpRequest::finalResponseMIMETypeWithFallback() const {
  AtomicString finalType = finalResponseMIMEType();
  if (!finalType.isEmpty())
    return finalType;

  // FIXME: This fallback is not specified in the final MIME type algorithm
  // of the XHR spec. Move this to more appropriate place.
  return AtomicString("text/xml");
}

bool XMLHttpRequest::responseIsXML() const {
  return DOMImplementation::isXMLMIMEType(finalResponseMIMETypeWithFallback());
}

bool XMLHttpRequest::responseIsHTML() const {
  return equalIgnoringCase(finalResponseMIMEType(), "text/html");
}

int XMLHttpRequest::status() const {
  if (m_state == kUnsent || m_state == kOpened || m_error)
    return 0;

  if (m_response.httpStatusCode())
    return m_response.httpStatusCode();

  return 0;
}

String XMLHttpRequest::statusText() const {
  if (m_state == kUnsent || m_state == kOpened || m_error)
    return String();

  if (!m_response.httpStatusText().isNull())
    return m_response.httpStatusText();

  return String();
}

void XMLHttpRequest::didFail(const ResourceError& error) {
  NETWORK_DVLOG(1) << this << " didFail()";
  ScopedEventDispatchProtect protect(&m_eventDispatchRecursionLevel);

  // If we are already in an error state, for instance we called abort(), bail
  // out early.
  if (m_error)
    return;

  // Internally, access check violations are considered `cancellations`, but
  // at least the mixed-content and CSP specs require them to be surfaced as
  // network errors to the page. See:
  //   [1] https://www.w3.org/TR/mixed-content/#algorithms,
  //   [2] https://www.w3.org/TR/CSP3/#fetch-integration.
  if (error.isCancellation() && !error.isAccessCheck()) {
    handleDidCancel();
    return;
  }

  if (error.isTimeout()) {
    handleDidTimeout();
    return;
  }

  // Network failures are already reported to Web Inspector by ResourceLoader.
  if (error.domain() == errorDomainBlinkInternal)
    logConsoleError(getExecutionContext(), "XMLHttpRequest cannot load " +
                                               error.failingURL() + ". " +
                                               error.localizedDescription());

  handleNetworkError();
}

void XMLHttpRequest::didFailRedirectCheck() {
  NETWORK_DVLOG(1) << this << " didFailRedirectCheck()";
  ScopedEventDispatchProtect protect(&m_eventDispatchRecursionLevel);

  handleNetworkError();
}

void XMLHttpRequest::didFinishLoading(unsigned long identifier, double) {
  NETWORK_DVLOG(1) << this << " didFinishLoading(" << identifier << ")";
  ScopedEventDispatchProtect protect(&m_eventDispatchRecursionLevel);

  if (m_error)
    return;

  if (m_state < kHeadersReceived)
    changeState(kHeadersReceived);

  if (m_downloadingToFile && m_responseTypeCode != ResponseTypeBlob &&
      m_lengthDownloadedToFile) {
    DCHECK_EQ(kLoading, m_state);
    // In this case, we have sent the request with DownloadToFile true,
    // but the user changed the response type after that. Hence we need to
    // read the response data and provide it to this object.
    m_blobLoader = BlobLoader::create(this, createBlobDataHandleFromResponse());
  } else {
    didFinishLoadingInternal();
  }
}

void XMLHttpRequest::didFinishLoadingInternal() {
  if (m_responseDocumentParser) {
    // |DocumentParser::finish()| tells the parser that we have reached end of
    // the data.  When using |HTMLDocumentParser|, which works asynchronously,
    // we do not have the complete document just after the
    // |DocumentParser::finish()| call.  Wait for the parser to call us back in
    // |notifyParserStopped| to progress state.
    m_responseDocumentParser->finish();
    DCHECK(m_responseDocument);
    return;
  }

  if (m_decoder) {
    auto text = m_decoder->flush();
    if (!text.isEmpty() && !m_responseTextOverflow) {
      m_responseText = m_responseText.concatenateWith(text);
      m_responseTextOverflow = m_responseText.isEmpty();
    }
  }

  clearVariablesForLoading();
  endLoading();
}

void XMLHttpRequest::didFinishLoadingFromBlob() {
  NETWORK_DVLOG(1) << this << " didFinishLoadingFromBlob";
  ScopedEventDispatchProtect protect(&m_eventDispatchRecursionLevel);

  didFinishLoadingInternal();
}

void XMLHttpRequest::didFailLoadingFromBlob() {
  NETWORK_DVLOG(1) << this << " didFailLoadingFromBlob()";
  ScopedEventDispatchProtect protect(&m_eventDispatchRecursionLevel);

  if (m_error)
    return;
  handleNetworkError();
}

PassRefPtr<BlobDataHandle> XMLHttpRequest::createBlobDataHandleFromResponse() {
  DCHECK(m_downloadingToFile);
  std::unique_ptr<BlobData> blobData = BlobData::create();
  String filePath = m_response.downloadedFilePath();
  // If we errored out or got no data, we return an empty handle.
  if (!filePath.isEmpty() && m_lengthDownloadedToFile) {
    blobData->appendFile(filePath, 0, m_lengthDownloadedToFile,
                         invalidFileTime());
    // FIXME: finalResponseMIMETypeWithFallback() defaults to
    // text/xml which may be incorrect. Replace it with
    // finalResponseMIMEType() after compatibility investigation.
    blobData->setContentType(finalResponseMIMETypeWithFallback().lower());
  }
  return BlobDataHandle::create(std::move(blobData), m_lengthDownloadedToFile);
}

void XMLHttpRequest::notifyParserStopped() {
  ScopedEventDispatchProtect protect(&m_eventDispatchRecursionLevel);

  // This should only be called when response document is parsed asynchronously.
  DCHECK(m_responseDocumentParser);
  DCHECK(!m_responseDocumentParser->isParsing());

  // Do nothing if we are called from |internalAbort()|.
  if (m_error)
    return;

  clearVariablesForLoading();

  m_responseDocument->implicitClose();

  if (!m_responseDocument->wellFormed())
    m_responseDocument = nullptr;

  m_parsedResponse = true;

  endLoading();
}

void XMLHttpRequest::endLoading() {
  InspectorInstrumentation::didFinishXHRLoading(getExecutionContext(), this,
                                                this, m_method, m_url);

  if (m_loader) {
    const bool hasError = m_error;
    // Set |m_error| in order to suppress the cancel notification (see
    // XMLHttpRequest::didFail).
    m_error = true;
    m_loader->cancel();
    m_error = hasError;
    m_loader = nullptr;
  }

  m_sendFlag = false;
  changeState(kDone);

  if (!getExecutionContext() || !getExecutionContext()->isDocument() ||
      !document() || !document()->frame() || !document()->frame()->page())
    return;

  if (status() >= 200 && status() < 300) {
    document()->frame()->page()->chromeClient().ajaxSucceeded(
        document()->frame());
  }
}

void XMLHttpRequest::didSendData(unsigned long long bytesSent,
                                 unsigned long long totalBytesToBeSent) {
  NETWORK_DVLOG(1) << this << " didSendData(" << bytesSent << ", "
                   << totalBytesToBeSent << ")";
  ScopedEventDispatchProtect protect(&m_eventDispatchRecursionLevel);

  if (!m_upload)
    return;

  if (m_uploadEventsAllowed)
    m_upload->dispatchProgressEvent(bytesSent, totalBytesToBeSent);

  if (bytesSent == totalBytesToBeSent && !m_uploadComplete) {
    m_uploadComplete = true;
    if (m_uploadEventsAllowed)
      m_upload->dispatchEventAndLoadEnd(EventTypeNames::load, true, bytesSent,
                                        totalBytesToBeSent);
  }
}

void XMLHttpRequest::didReceiveResponse(
    unsigned long identifier,
    const ResourceResponse& response,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  ALLOW_UNUSED_LOCAL(handle);
  DCHECK(!handle);
  NETWORK_DVLOG(1) << this << " didReceiveResponse(" << identifier << ")";
  ScopedEventDispatchProtect protect(&m_eventDispatchRecursionLevel);

  m_response = response;
  if (!m_mimeTypeOverride.isEmpty()) {
    m_response.setHTTPHeaderField(HTTPNames::Content_Type, m_mimeTypeOverride);
    m_finalResponseCharset = extractCharsetFromMediaType(m_mimeTypeOverride);
  }

  if (m_finalResponseCharset.isEmpty())
    m_finalResponseCharset = response.textEncodingName();
}

void XMLHttpRequest::parseDocumentChunk(const char* data, unsigned len) {
  if (!m_responseDocumentParser) {
    DCHECK(!m_responseDocument);
    initResponseDocument();
    if (!m_responseDocument)
      return;

    m_responseDocumentParser =
        m_responseDocument->implicitOpen(AllowAsynchronousParsing);
    m_responseDocumentParser->addClient(this);
  }
  DCHECK(m_responseDocumentParser);

  if (m_responseDocumentParser->needsDecoder())
    m_responseDocumentParser->setDecoder(createDecoder());

  m_responseDocumentParser->appendBytes(data, len);
}

std::unique_ptr<TextResourceDecoder> XMLHttpRequest::createDecoder() const {
  if (m_responseTypeCode == ResponseTypeJSON)
    return TextResourceDecoder::create("application/json", "UTF-8");

  if (!m_finalResponseCharset.isEmpty())
    return TextResourceDecoder::create("text/plain", m_finalResponseCharset);

  // allow TextResourceDecoder to look inside the m_response if it's XML or HTML
  if (responseIsXML()) {
    std::unique_ptr<TextResourceDecoder> decoder =
        TextResourceDecoder::create("application/xml");
    // Don't stop on encoding errors, unlike it is done for other kinds
    // of XML resources. This matches the behavior of previous WebKit
    // versions, Firefox and Opera.
    decoder->useLenientXMLDecoding();

    return decoder;
  }

  if (responseIsHTML())
    return TextResourceDecoder::create("text/html", "UTF-8");

  return TextResourceDecoder::create("text/plain", "UTF-8");
}

void XMLHttpRequest::didReceiveData(const char* data, unsigned len) {
  ScopedEventDispatchProtect protect(&m_eventDispatchRecursionLevel);
  if (m_error)
    return;

  if (m_state < kHeadersReceived)
    changeState(kHeadersReceived);

  // We need to check for |m_error| again, because |changeState| may trigger
  // readystatechange, and user javascript can cause |abort()|.
  if (m_error)
    return;

  if (!len)
    return;

  if (m_responseTypeCode == ResponseTypeDocument && responseIsHTML()) {
    parseDocumentChunk(data, len);
  } else if (m_responseTypeCode == ResponseTypeDefault ||
             m_responseTypeCode == ResponseTypeText ||
             m_responseTypeCode == ResponseTypeJSON ||
             m_responseTypeCode == ResponseTypeDocument) {
    if (!m_decoder)
      m_decoder = createDecoder();

    auto text = m_decoder->decode(data, len);
    if (!text.isEmpty() && !m_responseTextOverflow) {
      m_responseText = m_responseText.concatenateWith(text);
      m_responseTextOverflow = m_responseText.isEmpty();
    }
  } else if (m_responseTypeCode == ResponseTypeArrayBuffer ||
             m_responseTypeCode == ResponseTypeBlob) {
    // Buffer binary data.
    if (!m_binaryResponseBuilder)
      m_binaryResponseBuilder = SharedBuffer::create();
    m_binaryResponseBuilder->append(data, len);
  }

  if (m_blobLoader) {
    // In this case, the data is provided by m_blobLoader. As progress
    // events are already fired, we should return here.
    return;
  }
  trackProgress(len);
}

void XMLHttpRequest::didDownloadData(int dataLength) {
  ScopedEventDispatchProtect protect(&m_eventDispatchRecursionLevel);
  if (m_error)
    return;

  DCHECK(m_downloadingToFile);

  if (m_state < kHeadersReceived)
    changeState(kHeadersReceived);

  if (!dataLength)
    return;

  // readystatechange event handler may do something to put this XHR in error
  // state. We need to check m_error again here.
  if (m_error)
    return;

  m_lengthDownloadedToFile += dataLength;

  trackProgress(dataLength);
}

void XMLHttpRequest::handleDidTimeout() {
  NETWORK_DVLOG(1) << this << " handleDidTimeout()";

  // Response is cleared next, save needed progress event data.
  long long expectedLength = m_response.expectedContentLength();
  long long receivedLength = m_receivedLength;

  if (!internalAbort())
    return;

  handleRequestError(TimeoutError, EventTypeNames::timeout, receivedLength,
                     expectedLength);
}

void XMLHttpRequest::suspend() {
  m_progressEventThrottle->suspend();
}

void XMLHttpRequest::resume() {
  m_progressEventThrottle->resume();
}

void XMLHttpRequest::contextDestroyed() {
  InspectorInstrumentation::didFailXHRLoading(getExecutionContext(), this, this,
                                              m_method, m_url);
  m_progressEventThrottle->stop();
  internalAbort();
}

bool XMLHttpRequest::hasPendingActivity() const {
  // Neither this object nor the JavaScript wrapper should be deleted while
  // a request is in progress because we need to keep the listeners alive,
  // and they are referenced by the JavaScript wrapper.
  // |m_loader| is non-null while request is active and ThreadableLoaderClient
  // callbacks may be called, and |m_responseDocumentParser| is non-null while
  // DocumentParserClient callbacks may be called.
  if (m_loader || m_responseDocumentParser)
    return true;
  return m_eventDispatchRecursionLevel > 0;
}

const AtomicString& XMLHttpRequest::interfaceName() const {
  return EventTargetNames::XMLHttpRequest;
}

ExecutionContext* XMLHttpRequest::getExecutionContext() const {
  return ActiveDOMObject::getExecutionContext();
}

DEFINE_TRACE(XMLHttpRequest) {
  visitor->trace(m_responseBlob);
  visitor->trace(m_loader);
  visitor->trace(m_responseDocument);
  visitor->trace(m_responseDocumentParser);
  visitor->trace(m_responseArrayBuffer);
  visitor->trace(m_progressEventThrottle);
  visitor->trace(m_upload);
  visitor->trace(m_blobLoader);
  XMLHttpRequestEventTarget::trace(visitor);
  DocumentParserClient::trace(visitor);
  ActiveDOMObject::trace(visitor);
}

DEFINE_TRACE_WRAPPERS(XMLHttpRequest) {
  visitor->traceWrappers(m_responseBlob);
  visitor->traceWrappers(m_responseDocument);
  visitor->traceWrappers(m_responseArrayBuffer);
  XMLHttpRequestEventTarget::traceWrappers(visitor);
}

std::ostream& operator<<(std::ostream& ostream, const XMLHttpRequest* xhr) {
  return ostream << "XMLHttpRequest " << static_cast<const void*>(xhr);
}

}  // namespace blink
