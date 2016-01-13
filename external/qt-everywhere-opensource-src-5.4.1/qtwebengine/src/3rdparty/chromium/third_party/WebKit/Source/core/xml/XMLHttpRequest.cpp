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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "core/xml/XMLHttpRequest.h"

#include "bindings/v8/ExceptionState.h"
#include "core/FetchInitiatorTypeNames.h"
#include "core/dom/ContextFeatures.h"
#include "core/dom/DOMImplementation.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/XMLDocument.h"
#include "core/editing/markup.h"
#include "core/events/Event.h"
#include "core/fetch/CrossOriginAccessControl.h"
#include "core/fileapi/Blob.h"
#include "core/fileapi/File.h"
#include "core/fileapi/Stream.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/DOMFormData.h"
#include "core/html/HTMLDocument.h"
#include "core/html/parser/TextResourceDecoder.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/loader/ThreadableLoader.h"
#include "core/xml/XMLHttpRequestProgressEvent.h"
#include "core/xml/XMLHttpRequestUpload.h"
#include "platform/Logging.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/SharedBuffer.h"
#include "platform/blob/BlobData.h"
#include "platform/network/HTTPParsers.h"
#include "platform/network/ParsedContentType.h"
#include "platform/network/ResourceError.h"
#include "platform/network/ResourceRequest.h"
#include "public/platform/Platform.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/ArrayBufferView.h"
#include "wtf/Assertions.h"
#include "wtf/RefCountedLeakCounter.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/CString.h"

namespace WebCore {

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, xmlHttpRequestCounter, ("XMLHttpRequest"));

// Histogram enum to see when we can deprecate xhr.send(ArrayBuffer).
enum XMLHttpRequestSendArrayBufferOrView {
    XMLHttpRequestSendArrayBuffer,
    XMLHttpRequestSendArrayBufferView,
    XMLHttpRequestSendArrayBufferOrViewMax,
};

struct XMLHttpRequestStaticData {
    WTF_MAKE_NONCOPYABLE(XMLHttpRequestStaticData); WTF_MAKE_FAST_ALLOCATED;
public:
    XMLHttpRequestStaticData();
    String m_proxyHeaderPrefix;
    String m_secHeaderPrefix;
    HashSet<String, CaseFoldingHash> m_forbiddenRequestHeaders;
};

XMLHttpRequestStaticData::XMLHttpRequestStaticData()
    : m_proxyHeaderPrefix("proxy-")
    , m_secHeaderPrefix("sec-")
{
    m_forbiddenRequestHeaders.add("accept-charset");
    m_forbiddenRequestHeaders.add("accept-encoding");
    m_forbiddenRequestHeaders.add("access-control-request-headers");
    m_forbiddenRequestHeaders.add("access-control-request-method");
    m_forbiddenRequestHeaders.add("connection");
    m_forbiddenRequestHeaders.add("content-length");
    m_forbiddenRequestHeaders.add("content-transfer-encoding");
    m_forbiddenRequestHeaders.add("cookie");
    m_forbiddenRequestHeaders.add("cookie2");
    m_forbiddenRequestHeaders.add("date");
    m_forbiddenRequestHeaders.add("expect");
    m_forbiddenRequestHeaders.add("host");
    m_forbiddenRequestHeaders.add("keep-alive");
    m_forbiddenRequestHeaders.add("origin");
    m_forbiddenRequestHeaders.add("referer");
    m_forbiddenRequestHeaders.add("te");
    m_forbiddenRequestHeaders.add("trailer");
    m_forbiddenRequestHeaders.add("transfer-encoding");
    m_forbiddenRequestHeaders.add("upgrade");
    m_forbiddenRequestHeaders.add("user-agent");
    m_forbiddenRequestHeaders.add("via");
}

static bool isSetCookieHeader(const AtomicString& name)
{
    return equalIgnoringCase(name, "set-cookie") || equalIgnoringCase(name, "set-cookie2");
}

static void replaceCharsetInMediaType(String& mediaType, const String& charsetValue)
{
    unsigned pos = 0, len = 0;

    findCharsetInMediaType(mediaType, pos, len);

    if (!len) {
        // When no charset found, do nothing.
        return;
    }

    // Found at least one existing charset, replace all occurrences with new charset.
    while (len) {
        mediaType.replace(pos, len, charsetValue);
        unsigned start = pos + charsetValue.length();
        findCharsetInMediaType(mediaType, pos, len, start);
    }
}

static const XMLHttpRequestStaticData* staticData = 0;

static const XMLHttpRequestStaticData* createXMLHttpRequestStaticData()
{
    staticData = new XMLHttpRequestStaticData;
    return staticData;
}

static const XMLHttpRequestStaticData* initializeXMLHttpRequestStaticData()
{
    // Uses dummy to avoid warnings about an unused variable.
    AtomicallyInitializedStatic(const XMLHttpRequestStaticData*, dummy = createXMLHttpRequestStaticData());
    return dummy;
}

static void logConsoleError(ExecutionContext* context, const String& message)
{
    if (!context)
        return;
    // FIXME: It's not good to report the bad usage without indicating what source line it came from.
    // We should pass additional parameters so we can tell the console where the mistake occurred.
    context->addConsoleMessage(JSMessageSource, ErrorMessageLevel, message);
}

PassRefPtrWillBeRawPtr<XMLHttpRequest> XMLHttpRequest::create(ExecutionContext* context, PassRefPtr<SecurityOrigin> securityOrigin)
{
    RefPtrWillBeRawPtr<XMLHttpRequest> xmlHttpRequest = adoptRefWillBeRefCountedGarbageCollected(new XMLHttpRequest(context, securityOrigin));
    xmlHttpRequest->suspendIfNeeded();

    return xmlHttpRequest.release();
}

XMLHttpRequest::XMLHttpRequest(ExecutionContext* context, PassRefPtr<SecurityOrigin> securityOrigin)
    : ActiveDOMObject(context)
    , m_async(true)
    , m_includeCredentials(false)
    , m_timeoutMilliseconds(0)
    , m_state(UNSENT)
    , m_createdDocument(false)
    , m_downloadedBlobLength(0)
    , m_error(false)
    , m_uploadEventsAllowed(true)
    , m_uploadComplete(false)
    , m_sameOriginRequest(true)
    , m_receivedLength(0)
    , m_lastSendLineNumber(0)
    , m_exceptionCode(0)
    , m_progressEventThrottle(this)
    , m_responseTypeCode(ResponseTypeDefault)
    , m_dropProtectionRunner(this, &XMLHttpRequest::dropProtection)
    , m_securityOrigin(securityOrigin)
{
    initializeXMLHttpRequestStaticData();
#ifndef NDEBUG
    xmlHttpRequestCounter.increment();
#endif
    ScriptWrappable::init(this);
}

XMLHttpRequest::~XMLHttpRequest()
{
#ifndef NDEBUG
    xmlHttpRequestCounter.decrement();
#endif
}

Document* XMLHttpRequest::document() const
{
    ASSERT(executionContext()->isDocument());
    return toDocument(executionContext());
}

SecurityOrigin* XMLHttpRequest::securityOrigin() const
{
    return m_securityOrigin ? m_securityOrigin.get() : executionContext()->securityOrigin();
}

XMLHttpRequest::State XMLHttpRequest::readyState() const
{
    return m_state;
}

ScriptString XMLHttpRequest::responseText(ExceptionState& exceptionState)
{
    if (m_responseTypeCode != ResponseTypeDefault && m_responseTypeCode != ResponseTypeText) {
        exceptionState.throwDOMException(InvalidStateError, "The value is only accessible if the object's 'responseType' is '' or 'text' (was '" + responseType() + "').");
        return ScriptString();
    }
    if (m_error || (m_state != LOADING && m_state != DONE))
        return ScriptString();
    return m_responseText;
}

ScriptString XMLHttpRequest::responseJSONSource()
{
    ASSERT(m_responseTypeCode == ResponseTypeJSON);

    if (m_error || m_state != DONE)
        return ScriptString();
    return m_responseText;
}

Document* XMLHttpRequest::responseXML(ExceptionState& exceptionState)
{
    if (m_responseTypeCode != ResponseTypeDefault && m_responseTypeCode != ResponseTypeDocument) {
        exceptionState.throwDOMException(InvalidStateError, "The value is only accessible if the object's 'responseType' is '' or 'document' (was '" + responseType() + "').");
        return 0;
    }

    if (m_error || m_state != DONE)
        return 0;

    if (!m_createdDocument) {
        AtomicString mimeType = responseMIMEType();
        bool isHTML = equalIgnoringCase(mimeType, "text/html");

        // The W3C spec requires the final MIME type to be some valid XML type, or text/html.
        // If it is text/html, then the responseType of "document" must have been supplied explicitly.
        if ((m_response.isHTTP() && !responseIsXML() && !isHTML)
            || (isHTML && m_responseTypeCode == ResponseTypeDefault)
            || executionContext()->isWorkerGlobalScope()) {
            m_responseDocument = nullptr;
        } else {
            DocumentInit init = DocumentInit::fromContext(document()->contextDocument(), m_url);
            if (isHTML)
                m_responseDocument = HTMLDocument::create(init);
            else
                m_responseDocument = XMLDocument::create(init);
            // FIXME: Set Last-Modified.
            m_responseDocument->setContent(m_responseText.flattenToString());
            m_responseDocument->setSecurityOrigin(securityOrigin());
            m_responseDocument->setContextFeatures(document()->contextFeatures());
            m_responseDocument->setMimeType(mimeType);
            if (!m_responseDocument->wellFormed())
                m_responseDocument = nullptr;
        }
        m_createdDocument = true;
    }

    return m_responseDocument.get();
}

Blob* XMLHttpRequest::responseBlob()
{
    ASSERT(m_responseTypeCode == ResponseTypeBlob);
    ASSERT(!m_binaryResponseBuilder.get());

    // We always return null before DONE.
    if (m_error || m_state != DONE)
        return 0;

    if (!m_responseBlob) {
        // When responseType is set to "blob", we redirect the downloaded data
        // to a file-handle directly in the browser process. We get the
        // file-path from the ResourceResponse directly instead of copying the
        // bytes between the browser and the renderer.
        OwnPtr<BlobData> blobData = BlobData::create();
        String filePath = m_response.downloadedFilePath();
        // If we errored out or got no data, we still return a blob, just an
        // empty one.
        if (!filePath.isEmpty() && m_downloadedBlobLength) {
            blobData->appendFile(filePath);
            blobData->setContentType(responseMIMEType()); // responseMIMEType defaults to text/xml which may be incorrect.
        }
        m_responseBlob = Blob::create(BlobDataHandle::create(blobData.release(), m_downloadedBlobLength));
    }

    return m_responseBlob.get();
}

ArrayBuffer* XMLHttpRequest::responseArrayBuffer()
{
    ASSERT(m_responseTypeCode == ResponseTypeArrayBuffer);

    if (m_error || m_state != DONE)
        return 0;

    if (!m_responseArrayBuffer.get()) {
        if (m_binaryResponseBuilder.get() && m_binaryResponseBuilder->size() > 0) {
            m_responseArrayBuffer = m_binaryResponseBuilder->getAsArrayBuffer();
            if (!m_responseArrayBuffer) {
                // m_binaryResponseBuilder failed to allocate an ArrayBuffer.
                // We need to crash the renderer since there's no way defined in
                // the spec to tell this to the user.
                CRASH();
            }
            m_binaryResponseBuilder.clear();
        } else {
            m_responseArrayBuffer = ArrayBuffer::create(static_cast<void*>(0), 0);
        }
    }

    return m_responseArrayBuffer.get();
}

Stream* XMLHttpRequest::responseStream()
{
    ASSERT(m_responseTypeCode == ResponseTypeStream);

    if (m_error || (m_state != LOADING && m_state != DONE))
        return 0;

    return m_responseStream.get();
}

void XMLHttpRequest::setTimeout(unsigned long timeout, ExceptionState& exceptionState)
{
    // FIXME: Need to trigger or update the timeout Timer here, if needed. http://webkit.org/b/98156
    // XHR2 spec, 4.7.3. "This implies that the timeout attribute can be set while fetching is in progress. If that occurs it will still be measured relative to the start of fetching."
    if (executionContext()->isDocument() && !m_async) {
        exceptionState.throwDOMException(InvalidAccessError, "Timeouts cannot be set for synchronous requests made from a document.");
        return;
    }
    m_timeoutMilliseconds = timeout;
}

void XMLHttpRequest::setResponseType(const String& responseType, ExceptionState& exceptionState)
{
    if (m_state >= LOADING) {
        exceptionState.throwDOMException(InvalidStateError, "The response type cannot be set if the object's state is LOADING or DONE.");
        return;
    }

    // Newer functionality is not available to synchronous requests in window contexts, as a spec-mandated
    // attempt to discourage synchronous XHR use. responseType is one such piece of functionality.
    if (!m_async && executionContext()->isDocument()) {
        exceptionState.throwDOMException(InvalidAccessError, "The response type can only be changed for asynchronous HTTP requests made from a document.");
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
    } else if (responseType == "stream") {
        if (RuntimeEnabledFeatures::streamEnabled())
            m_responseTypeCode = ResponseTypeStream;
        else
            return;
    } else {
        ASSERT_NOT_REACHED();
    }
}

String XMLHttpRequest::responseType()
{
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
    case ResponseTypeStream:
        return "stream";
    }
    return "";
}

String XMLHttpRequest::responseURL()
{
    return m_response.url().string();
}

XMLHttpRequestUpload* XMLHttpRequest::upload()
{
    if (!m_upload)
        m_upload = XMLHttpRequestUpload::create(this);
    return m_upload.get();
}

void XMLHttpRequest::trackProgress(int length)
{
    m_receivedLength += length;

    if (m_async)
        dispatchProgressEventFromSnapshot(EventTypeNames::progress);

    if (m_state != LOADING) {
        changeState(LOADING);
    } else {
        // Firefox calls readyStateChanged every time it receives data. Do
        // the same to align with Firefox.
        //
        // FIXME: Make our implementation and the spec consistent. This
        // behavior was needed when the progress event was not available.
        dispatchReadyStateChangeEvent();
    }
}

void XMLHttpRequest::changeState(State newState)
{
    if (m_state != newState) {
        m_state = newState;
        dispatchReadyStateChangeEvent();
    }
}

void XMLHttpRequest::dispatchReadyStateChangeEvent()
{
    if (!executionContext())
        return;

    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willDispatchXHRReadyStateChangeEvent(executionContext(), this);

    if (m_async || (m_state <= OPENED || m_state == DONE)) {
        TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "XHRReadyStateChange", "data", InspectorXhrReadyStateChangeEvent::data(executionContext(), this));
        TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline.stack"), "CallStack", "stack", InspectorCallStackEvent::currentCallStack());
        ProgressEventAction flushAction = DoNotFlushProgressEvent;
        if (m_state == DONE) {
            if (m_error)
                flushAction = FlushDeferredProgressEvent;
            else
                flushAction = FlushProgressEvent;
        }
        m_progressEventThrottle.dispatchReadyStateChangeEvent(XMLHttpRequestProgressEvent::create(EventTypeNames::readystatechange), flushAction);
        TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "UpdateCounters", "data", InspectorUpdateCountersEvent::data());
    }

    InspectorInstrumentation::didDispatchXHRReadyStateChangeEvent(cookie);
    if (m_state == DONE && !m_error) {
        {
            TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "XHRLoad", "data", InspectorXhrLoadEvent::data(executionContext(), this));
            TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline.stack"), "CallStack", "stack", InspectorCallStackEvent::currentCallStack());
            InspectorInstrumentationCookie cookie = InspectorInstrumentation::willDispatchXHRLoadEvent(executionContext(), this);
            dispatchProgressEventFromSnapshot(EventTypeNames::load);
            InspectorInstrumentation::didDispatchXHRLoadEvent(cookie);
        }
        dispatchProgressEventFromSnapshot(EventTypeNames::loadend);
        TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "UpdateCounters", "data", InspectorUpdateCountersEvent::data());
    }
}

void XMLHttpRequest::setWithCredentials(bool value, ExceptionState& exceptionState)
{
    if (m_state > OPENED || m_loader) {
        exceptionState.throwDOMException(InvalidStateError,  "The value may only be set if the object's state is UNSENT or OPENED.");
        return;
    }

    // FIXME: According to XMLHttpRequest Level 2 we should throw InvalidAccessError exception here.
    // However for time being only print warning message to warn web developers.
    if (!m_async)
        UseCounter::countDeprecation(executionContext(), UseCounter::SyncXHRWithCredentials);

    m_includeCredentials = value;
}

bool XMLHttpRequest::isAllowedHTTPMethod(const String& method)
{
    return !equalIgnoringCase(method, "TRACE")
        && !equalIgnoringCase(method, "TRACK")
        && !equalIgnoringCase(method, "CONNECT");
}

AtomicString XMLHttpRequest::uppercaseKnownHTTPMethod(const AtomicString& method)
{
    const char* const methods[] = {
        "COPY",
        "DELETE",
        "GET",
        "HEAD",
        "INDEX",
        "LOCK",
        "M-POST",
        "MKCOL",
        "MOVE",
        "OPTIONS",
        "POST",
        "PROPFIND",
        "PROPPATCH",
        "PUT",
        "UNLOCK" };
    for (unsigned i = 0; i < WTF_ARRAY_LENGTH(methods); ++i) {
        if (equalIgnoringCase(method, methods[i])) {
            // Don't bother allocating a new string if it's already all uppercase.
            if (method == methods[i])
                return method;
            return methods[i];
        }
    }
    return method;
}

bool XMLHttpRequest::isAllowedHTTPHeader(const String& name)
{
    initializeXMLHttpRequestStaticData();
    return !staticData->m_forbiddenRequestHeaders.contains(name) && !name.startsWith(staticData->m_proxyHeaderPrefix, false)
        && !name.startsWith(staticData->m_secHeaderPrefix, false);
}

void XMLHttpRequest::open(const AtomicString& method, const KURL& url, ExceptionState& exceptionState)
{
    open(method, url, true, exceptionState);
}

void XMLHttpRequest::open(const AtomicString& method, const KURL& url, bool async, ExceptionState& exceptionState)
{
    WTF_LOG(Network, "XMLHttpRequest %p open('%s', '%s', %d)", this, method.utf8().data(), url.elidedString().utf8().data(), async);

    if (!internalAbort())
        return;

    State previousState = m_state;
    m_state = UNSENT;
    m_error = false;
    m_uploadComplete = false;

    // clear stuff from possible previous load
    clearResponse();
    clearRequest();

    ASSERT(m_state == UNSENT);

    if (!isValidHTTPToken(method)) {
        exceptionState.throwDOMException(SyntaxError, "'" + method + "' is not a valid HTTP method.");
        return;
    }

    if (!isAllowedHTTPMethod(method)) {
        exceptionState.throwSecurityError("'" + method + "' HTTP method is unsupported.");
        return;
    }

    if (!ContentSecurityPolicy::shouldBypassMainWorld(executionContext()) && !executionContext()->contentSecurityPolicy()->allowConnectToSource(url)) {
        // We can safely expose the URL to JavaScript, as these checks happen synchronously before redirection. JavaScript receives no new information.
        exceptionState.throwSecurityError("Refused to connect to '" + url.elidedString() + "' because it violates the document's Content Security Policy.");
        return;
    }

    if (!async && executionContext()->isDocument()) {
        if (document()->settings() && !document()->settings()->syncXHRInDocumentsEnabled()) {
            exceptionState.throwDOMException(InvalidAccessError, "Synchronous requests are disabled for this page.");
            return;
        }

        // Newer functionality is not available to synchronous requests in window contexts, as a spec-mandated
        // attempt to discourage synchronous XHR use. responseType is one such piece of functionality.
        if (m_responseTypeCode != ResponseTypeDefault) {
            exceptionState.throwDOMException(InvalidAccessError, "Synchronous requests from a document must not set a response type.");
            return;
        }

        // Similarly, timeouts are disabled for synchronous requests as well.
        if (m_timeoutMilliseconds > 0) {
            exceptionState.throwDOMException(InvalidAccessError, "Synchronous requests must not set a timeout.");
            return;
        }
    }

    m_method = uppercaseKnownHTTPMethod(method);

    m_url = url;

    m_async = async;

    ASSERT(!m_loader);

    // Check previous state to avoid dispatching readyState event
    // when calling open several times in a row.
    if (previousState != OPENED)
        changeState(OPENED);
    else
        m_state = OPENED;
}

void XMLHttpRequest::open(const AtomicString& method, const KURL& url, bool async, const String& user, ExceptionState& exceptionState)
{
    KURL urlWithCredentials(url);
    urlWithCredentials.setUser(user);

    open(method, urlWithCredentials, async, exceptionState);
}

void XMLHttpRequest::open(const AtomicString& method, const KURL& url, bool async, const String& user, const String& password, ExceptionState& exceptionState)
{
    KURL urlWithCredentials(url);
    urlWithCredentials.setUser(user);
    urlWithCredentials.setPass(password);

    open(method, urlWithCredentials, async, exceptionState);
}

bool XMLHttpRequest::initSend(ExceptionState& exceptionState)
{
    if (!executionContext())
        return false;

    if (m_state != OPENED || m_loader) {
        exceptionState.throwDOMException(InvalidStateError, "The object's state must be OPENED.");
        return false;
    }

    m_error = false;
    return true;
}

void XMLHttpRequest::send(ExceptionState& exceptionState)
{
    send(String(), exceptionState);
}

bool XMLHttpRequest::areMethodAndURLValidForSend()
{
    return m_method != "GET" && m_method != "HEAD" && m_url.protocolIsInHTTPFamily();
}

void XMLHttpRequest::send(Document* document, ExceptionState& exceptionState)
{
    WTF_LOG(Network, "XMLHttpRequest %p send() Document %p", this, document);

    ASSERT(document);

    if (!initSend(exceptionState))
        return;

    RefPtr<FormData> httpBody;

    if (areMethodAndURLValidForSend()) {
        if (getRequestHeader("Content-Type").isEmpty()) {
            // FIXME: this should include the charset used for encoding.
            setRequestHeaderInternal("Content-Type", "application/xml");
        }

        // FIXME: According to XMLHttpRequest Level 2, this should use the Document.innerHTML algorithm
        // from the HTML5 specification to serialize the document.
        String body = createMarkup(document);

        // FIXME: This should use value of document.inputEncoding to determine the encoding to use.
        httpBody = FormData::create(UTF8Encoding().encode(body, WTF::EntitiesForUnencodables));
        if (m_upload)
            httpBody->setAlwaysStream(true);
    }

    createRequest(httpBody.release(), exceptionState);
}

void XMLHttpRequest::send(const String& body, ExceptionState& exceptionState)
{
    WTF_LOG(Network, "XMLHttpRequest %p send() String '%s'", this, body.utf8().data());

    if (!initSend(exceptionState))
        return;

    RefPtr<FormData> httpBody;

    if (!body.isNull() && areMethodAndURLValidForSend()) {
        String contentType = getRequestHeader("Content-Type");
        if (contentType.isEmpty()) {
            setRequestHeaderInternal("Content-Type", "text/plain;charset=UTF-8");
        } else {
            replaceCharsetInMediaType(contentType, "UTF-8");
            m_requestHeaders.set("Content-Type", AtomicString(contentType));
        }

        httpBody = FormData::create(UTF8Encoding().encode(body, WTF::EntitiesForUnencodables));
        if (m_upload)
            httpBody->setAlwaysStream(true);
    }

    createRequest(httpBody.release(), exceptionState);
}

void XMLHttpRequest::send(Blob* body, ExceptionState& exceptionState)
{
    WTF_LOG(Network, "XMLHttpRequest %p send() Blob '%s'", this, body->uuid().utf8().data());

    if (!initSend(exceptionState))
        return;

    RefPtr<FormData> httpBody;

    if (areMethodAndURLValidForSend()) {
        if (getRequestHeader("Content-Type").isEmpty()) {
            const String& blobType = body->type();
            if (!blobType.isEmpty() && isValidContentType(blobType)) {
                setRequestHeaderInternal("Content-Type", AtomicString(blobType));
            } else {
                // From FileAPI spec, whenever media type cannot be determined,
                // empty string must be returned.
                setRequestHeaderInternal("Content-Type", "");
            }
        }

        // FIXME: add support for uploading bundles.
        httpBody = FormData::create();
        if (body->hasBackingFile()) {
            File* file = toFile(body);
            if (!file->path().isEmpty())
                httpBody->appendFile(file->path());
            else if (!file->fileSystemURL().isEmpty())
                httpBody->appendFileSystemURL(file->fileSystemURL());
            else
                ASSERT_NOT_REACHED();
        } else {
            httpBody->appendBlob(body->uuid(), body->blobDataHandle());
        }
    }

    createRequest(httpBody.release(), exceptionState);
}

void XMLHttpRequest::send(DOMFormData* body, ExceptionState& exceptionState)
{
    WTF_LOG(Network, "XMLHttpRequest %p send() DOMFormData %p", this, body);

    if (!initSend(exceptionState))
        return;

    RefPtr<FormData> httpBody;

    if (areMethodAndURLValidForSend()) {
        httpBody = body->createMultiPartFormData();

        if (getRequestHeader("Content-Type").isEmpty()) {
            AtomicString contentType = AtomicString("multipart/form-data; boundary=", AtomicString::ConstructFromLiteral) + httpBody->boundary().data();
            setRequestHeaderInternal("Content-Type", contentType);
        }
    }

    createRequest(httpBody.release(), exceptionState);
}

void XMLHttpRequest::send(ArrayBuffer* body, ExceptionState& exceptionState)
{
    WTF_LOG(Network, "XMLHttpRequest %p send() ArrayBuffer %p", this, body);

    String consoleMessage("ArrayBuffer is deprecated in XMLHttpRequest.send(). Use ArrayBufferView instead.");
    executionContext()->addConsoleMessage(JSMessageSource, WarningMessageLevel, consoleMessage);

    blink::Platform::current()->histogramEnumeration("WebCore.XHR.send.ArrayBufferOrView", XMLHttpRequestSendArrayBuffer, XMLHttpRequestSendArrayBufferOrViewMax);

    sendBytesData(body->data(), body->byteLength(), exceptionState);
}

void XMLHttpRequest::send(ArrayBufferView* body, ExceptionState& exceptionState)
{
    WTF_LOG(Network, "XMLHttpRequest %p send() ArrayBufferView %p", this, body);

    blink::Platform::current()->histogramEnumeration("WebCore.XHR.send.ArrayBufferOrView", XMLHttpRequestSendArrayBufferView, XMLHttpRequestSendArrayBufferOrViewMax);

    sendBytesData(body->baseAddress(), body->byteLength(), exceptionState);
}

void XMLHttpRequest::sendBytesData(const void* data, size_t length, ExceptionState& exceptionState)
{
    if (!initSend(exceptionState))
        return;

    RefPtr<FormData> httpBody;

    if (areMethodAndURLValidForSend()) {
        httpBody = FormData::create(data, length);
        if (m_upload)
            httpBody->setAlwaysStream(true);
    }

    createRequest(httpBody.release(), exceptionState);
}

void XMLHttpRequest::sendForInspectorXHRReplay(PassRefPtr<FormData> formData, ExceptionState& exceptionState)
{
    createRequest(formData ? formData->deepCopy() : nullptr, exceptionState);
    m_exceptionCode = exceptionState.code();
}

void XMLHttpRequest::createRequest(PassRefPtr<FormData> httpBody, ExceptionState& exceptionState)
{
    // Only GET request is supported for blob URL.
    if (m_url.protocolIs("blob") && m_method != "GET") {
        exceptionState.throwDOMException(NetworkError, "'GET' is the only method allowed for 'blob:' URLs.");
        return;
    }

    // The presence of upload event listeners forces us to use preflighting because POSTing to an URL that does not
    // permit cross origin requests should look exactly like POSTing to an URL that does not respond at all.
    // Also, only async requests support upload progress events.
    bool uploadEvents = false;
    if (m_async) {
        dispatchProgressEvent(EventTypeNames::loadstart, 0, 0);
        if (httpBody && m_upload) {
            uploadEvents = m_upload->hasEventListeners();
            m_upload->dispatchEvent(XMLHttpRequestProgressEvent::create(EventTypeNames::loadstart));
        }
    }

    m_sameOriginRequest = securityOrigin()->canRequest(m_url);

    // We also remember whether upload events should be allowed for this request in case the upload listeners are
    // added after the request is started.
    m_uploadEventsAllowed = m_sameOriginRequest || uploadEvents || !isSimpleCrossOriginAccessRequest(m_method, m_requestHeaders);

    ASSERT(executionContext());
    ExecutionContext& executionContext = *this->executionContext();

    ResourceRequest request(m_url);
    request.setHTTPMethod(m_method);
    request.setTargetType(ResourceRequest::TargetIsXHR);

    InspectorInstrumentation::willLoadXHR(&executionContext, this, this, m_method, m_url, m_async, httpBody ? httpBody->deepCopy() : nullptr, m_requestHeaders, m_includeCredentials);

    if (httpBody) {
        ASSERT(m_method != "GET");
        ASSERT(m_method != "HEAD");
        request.setHTTPBody(httpBody);
    }

    if (m_requestHeaders.size() > 0)
        request.addHTTPHeaderFields(m_requestHeaders);

    ThreadableLoaderOptions options;
    options.preflightPolicy = uploadEvents ? ForcePreflight : ConsiderPreflight;
    options.crossOriginRequestPolicy = UseAccessControl;
    options.initiator = FetchInitiatorTypeNames::xmlhttprequest;
    options.contentSecurityPolicyEnforcement = ContentSecurityPolicy::shouldBypassMainWorld(&executionContext) ? DoNotEnforceContentSecurityPolicy : EnforceConnectSrcDirective;
    options.timeoutMilliseconds = m_timeoutMilliseconds;

    ResourceLoaderOptions resourceLoaderOptions;
    resourceLoaderOptions.allowCredentials = (m_sameOriginRequest || m_includeCredentials) ? AllowStoredCredentials : DoNotAllowStoredCredentials;
    resourceLoaderOptions.credentialsRequested = m_includeCredentials ? ClientRequestedCredentials : ClientDidNotRequestCredentials;
    resourceLoaderOptions.securityOrigin = securityOrigin();
    // TODO(tsepez): Specify TreatAsActiveContent per http://crbug.com/305303.
    resourceLoaderOptions.mixedContentBlockingTreatment = TreatAsPassiveContent;

    // When responseType is set to "blob", we redirect the downloaded data to a
    // file-handle directly.
    if (responseTypeCode() == ResponseTypeBlob) {
        request.setDownloadToFile(true);
        resourceLoaderOptions.dataBufferingPolicy = DoNotBufferData;
    }

    m_exceptionCode = 0;
    m_error = false;

    if (m_async) {
        if (m_upload)
            request.setReportUploadProgress(true);

        // ThreadableLoader::create can return null here, for example if we're no longer attached to a page.
        // This is true while running onunload handlers.
        // FIXME: Maybe we need to be able to send XMLHttpRequests from onunload, <http://bugs.webkit.org/show_bug.cgi?id=10904>.
        // FIXME: Maybe create() can return null for other reasons too?
        ASSERT(!m_loader);
        m_loader = ThreadableLoader::create(executionContext, this, request, options, resourceLoaderOptions);
        if (m_loader) {
            // Neither this object nor the JavaScript wrapper should be deleted while
            // a request is in progress because we need to keep the listeners alive,
            // and they are referenced by the JavaScript wrapper.
            setPendingActivity(this);
        }
    } else {
        // Use count for XHR synchronous requests.
        UseCounter::count(&executionContext, UseCounter::XMLHttpRequestSynchronous);
        ThreadableLoader::loadResourceSynchronously(executionContext, request, *this, options, resourceLoaderOptions);
    }

    if (!m_exceptionCode && m_error)
        m_exceptionCode = NetworkError;
    if (m_exceptionCode)
        exceptionState.throwDOMException(m_exceptionCode, "Failed to load '" + m_url.elidedString() + "'.");
}

void XMLHttpRequest::abort()
{
    WTF_LOG(Network, "XMLHttpRequest %p abort()", this);

    // internalAbort() calls dropProtection(), which may release the last reference.
    RefPtrWillBeRawPtr<XMLHttpRequest> protect(this);

    bool sendFlag = m_loader;

    // Response is cleared next, save needed progress event data.
    long long expectedLength = m_response.expectedContentLength();
    long long receivedLength = m_receivedLength;

    if (!internalAbort())
        return;

    clearResponse();

    // Clear headers as required by the spec
    m_requestHeaders.clear();

    if (!((m_state <= OPENED && !sendFlag) || m_state == DONE)) {
        ASSERT(!m_loader);
        handleRequestError(0, EventTypeNames::abort, receivedLength, expectedLength);
    }
    m_state = UNSENT;
}

void XMLHttpRequest::clearVariablesForLoading()
{
    m_decoder.clear();

    m_responseEncoding = String();
}

bool XMLHttpRequest::internalAbort(DropProtection async)
{
    m_error = true;

    clearVariablesForLoading();

    InspectorInstrumentation::didFailXHRLoading(executionContext(), this, this);

    if (m_responseStream && m_state != DONE)
        m_responseStream->abort();

    if (!m_loader)
        return true;

    // Cancelling the ThreadableLoader m_loader may result in calling
    // window.onload synchronously. If such an onload handler contains open()
    // call on the same XMLHttpRequest object, reentry happens. If m_loader
    // is left to be non 0, internalAbort() call for the inner open() makes
    // an extra dropProtection() call (when we're back to the outer open(),
    // we'll call dropProtection()). To avoid that, clears m_loader before
    // calling cancel.
    //
    // If, window.onload contains open() and send(), m_loader will be set to
    // non 0 value. So, we cannot continue the outer open(). In such case,
    // just abort the outer open() by returning false.
    RefPtr<ThreadableLoader> loader = m_loader.release();
    loader->cancel();

    // Save to a local variable since we're going to drop protection.
    bool newLoadStarted = m_loader;

    // If abort() called internalAbort() and a nested open() ended up
    // clearing the error flag, but didn't send(), make sure the error
    // flag is still set.
    if (!newLoadStarted)
        m_error = true;

    if (async == DropProtectionAsync)
        dropProtectionSoon();
    else
        dropProtection();

    return !newLoadStarted;
}

void XMLHttpRequest::clearResponse()
{
    // FIXME: when we add the support for multi-part XHR, we will have to
    // be careful with this initialization.
    m_receivedLength = 0;

    m_response = ResourceResponse();

    m_responseText.clear();

    m_createdDocument = false;
    m_responseDocument = nullptr;

    m_responseBlob = nullptr;
    m_downloadedBlobLength = 0;

    m_responseStream = nullptr;

    // These variables may referred by the response accessors. So, we can clear
    // this only when we clear the response holder variables above.
    m_binaryResponseBuilder.clear();
    m_responseArrayBuffer.clear();
}

void XMLHttpRequest::clearRequest()
{
    m_requestHeaders.clear();
}

void XMLHttpRequest::handleDidFailGeneric()
{
    clearResponse();
    clearRequest();

    m_error = true;
}

void XMLHttpRequest::dispatchProgressEvent(const AtomicString& type, long long receivedLength, long long expectedLength)
{
    bool lengthComputable = expectedLength > 0 && receivedLength <= expectedLength;
    unsigned long long loaded = receivedLength >= 0 ? static_cast<unsigned long long>(receivedLength) : 0;
    unsigned long long total = lengthComputable ? static_cast<unsigned long long>(expectedLength) : 0;

    m_progressEventThrottle.dispatchProgressEvent(type, lengthComputable, loaded, total);
}

void XMLHttpRequest::dispatchProgressEventFromSnapshot(const AtomicString& type)
{
    dispatchProgressEvent(type, m_receivedLength, m_response.expectedContentLength());
}

void XMLHttpRequest::handleNetworkError()
{
    WTF_LOG(Network, "XMLHttpRequest %p handleNetworkError()", this);

    // Response is cleared next, save needed progress event data.
    long long expectedLength = m_response.expectedContentLength();
    long long receivedLength = m_receivedLength;

    handleDidFailGeneric();
    handleRequestError(NetworkError, EventTypeNames::error, receivedLength, expectedLength);
    internalAbort();
}

void XMLHttpRequest::handleDidCancel()
{
    WTF_LOG(Network, "XMLHttpRequest %p handleDidCancel()", this);

    // Response is cleared next, save needed progress event data.
    long long expectedLength = m_response.expectedContentLength();
    long long receivedLength = m_receivedLength;

    handleDidFailGeneric();
    handleRequestError(AbortError, EventTypeNames::abort, receivedLength, expectedLength);
}

void XMLHttpRequest::handleRequestError(ExceptionCode exceptionCode, const AtomicString& type, long long receivedLength, long long expectedLength)
{
    WTF_LOG(Network, "XMLHttpRequest %p handleRequestError()", this);

    // The request error steps for event 'type' and exception 'exceptionCode'.

    if (!m_async && exceptionCode) {
        m_state = DONE;
        m_exceptionCode = exceptionCode;
        return;
    }
    // With m_error set, the state change steps are minimal: any pending
    // progress event is flushed + a readystatechange is dispatched.
    // No new progress events dispatched; as required, that happens at
    // the end here.
    ASSERT(m_error);
    changeState(DONE);

    if (!m_uploadComplete) {
        m_uploadComplete = true;
        if (m_upload && m_uploadEventsAllowed)
            m_upload->handleRequestError(type);
    }

    dispatchProgressEvent(EventTypeNames::progress, receivedLength, expectedLength);
    dispatchProgressEvent(type, receivedLength, expectedLength);
    dispatchProgressEvent(EventTypeNames::loadend, receivedLength, expectedLength);
}

void XMLHttpRequest::dropProtectionSoon()
{
    m_dropProtectionRunner.runAsync();
}

void XMLHttpRequest::dropProtection()
{
    unsetPendingActivity(this);
}

void XMLHttpRequest::overrideMimeType(const AtomicString& override)
{
    m_mimeTypeOverride = override;
}

void XMLHttpRequest::setRequestHeader(const AtomicString& name, const AtomicString& value, ExceptionState& exceptionState)
{
    if (m_state != OPENED || m_loader) {
        exceptionState.throwDOMException(InvalidStateError, "The object's state must be OPENED.");
        return;
    }

    if (!isValidHTTPToken(name)) {
        exceptionState.throwDOMException(SyntaxError, "'" + name + "' is not a valid HTTP header field name.");
        return;
    }

    if (!isValidHTTPHeaderValue(value)) {
        exceptionState.throwDOMException(SyntaxError, "'" + value + "' is not a valid HTTP header field value.");
        return;
    }

    // No script (privileged or not) can set unsafe headers.
    if (!isAllowedHTTPHeader(name)) {
        logConsoleError(executionContext(), "Refused to set unsafe header \"" + name + "\"");
        return;
    }

    setRequestHeaderInternal(name, value);
}

void XMLHttpRequest::setRequestHeaderInternal(const AtomicString& name, const AtomicString& value)
{
    HTTPHeaderMap::AddResult result = m_requestHeaders.add(name, value);
    if (!result.isNewEntry)
        result.storedValue->value = result.storedValue->value + ", " + value;
}

const AtomicString& XMLHttpRequest::getRequestHeader(const AtomicString& name) const
{
    return m_requestHeaders.get(name);
}

String XMLHttpRequest::getAllResponseHeaders() const
{
    if (m_state < HEADERS_RECEIVED || m_error)
        return "";

    StringBuilder stringBuilder;

    HTTPHeaderSet accessControlExposeHeaderSet;
    parseAccessControlExposeHeadersAllowList(m_response.httpHeaderField("Access-Control-Expose-Headers"), accessControlExposeHeaderSet);
    HTTPHeaderMap::const_iterator end = m_response.httpHeaderFields().end();
    for (HTTPHeaderMap::const_iterator it = m_response.httpHeaderFields().begin(); it!= end; ++it) {
        // Hide Set-Cookie header fields from the XMLHttpRequest client for these reasons:
        //     1) If the client did have access to the fields, then it could read HTTP-only
        //        cookies; those cookies are supposed to be hidden from scripts.
        //     2) There's no known harm in hiding Set-Cookie header fields entirely; we don't
        //        know any widely used technique that requires access to them.
        //     3) Firefox has implemented this policy.
        if (isSetCookieHeader(it->key) && !securityOrigin()->canLoadLocalResources())
            continue;

        if (!m_sameOriginRequest && !isOnAccessControlResponseHeaderWhitelist(it->key) && !accessControlExposeHeaderSet.contains(it->key))
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

const AtomicString& XMLHttpRequest::getResponseHeader(const AtomicString& name) const
{
    if (m_state < HEADERS_RECEIVED || m_error)
        return nullAtom;

    // See comment in getAllResponseHeaders above.
    if (isSetCookieHeader(name) && !securityOrigin()->canLoadLocalResources()) {
        logConsoleError(executionContext(), "Refused to get unsafe header \"" + name + "\"");
        return nullAtom;
    }

    HTTPHeaderSet accessControlExposeHeaderSet;
    parseAccessControlExposeHeadersAllowList(m_response.httpHeaderField("Access-Control-Expose-Headers"), accessControlExposeHeaderSet);

    if (!m_sameOriginRequest && !isOnAccessControlResponseHeaderWhitelist(name) && !accessControlExposeHeaderSet.contains(name)) {
        logConsoleError(executionContext(), "Refused to get unsafe header \"" + name + "\"");
        return nullAtom;
    }
    return m_response.httpHeaderField(name);
}

AtomicString XMLHttpRequest::responseMIMEType() const
{
    AtomicString mimeType = extractMIMETypeFromMediaType(m_mimeTypeOverride);
    if (mimeType.isEmpty()) {
        if (m_response.isHTTP())
            mimeType = extractMIMETypeFromMediaType(m_response.httpHeaderField("Content-Type"));
        else
            mimeType = m_response.mimeType();
    }
    if (mimeType.isEmpty())
        mimeType = AtomicString("text/xml", AtomicString::ConstructFromLiteral);

    return mimeType;
}

bool XMLHttpRequest::responseIsXML() const
{
    return DOMImplementation::isXMLMIMEType(responseMIMEType());
}

int XMLHttpRequest::status() const
{
    if (m_state == UNSENT || m_state == OPENED || m_error)
        return 0;

    if (m_response.httpStatusCode())
        return m_response.httpStatusCode();

    return 0;
}

String XMLHttpRequest::statusText() const
{
    if (m_state == UNSENT || m_state == OPENED || m_error)
        return String();

    if (!m_response.httpStatusText().isNull())
        return m_response.httpStatusText();

    return String();
}

void XMLHttpRequest::didFail(const ResourceError& error)
{
    WTF_LOG(Network, "XMLHttpRequest %p didFail()", this);

    // If we are already in an error state, for instance we called abort(), bail out early.
    if (m_error)
        return;

    if (error.isCancellation()) {
        handleDidCancel();
        return;
    }

    if (error.isTimeout()) {
        handleDidTimeout();
        return;
    }

    // Network failures are already reported to Web Inspector by ResourceLoader.
    if (error.domain() == errorDomainBlinkInternal)
        logConsoleError(executionContext(), "XMLHttpRequest cannot load " + error.failingURL() + ". " + error.localizedDescription());

    handleNetworkError();
}

void XMLHttpRequest::didFailRedirectCheck()
{
    WTF_LOG(Network, "XMLHttpRequest %p didFailRedirectCheck()", this);

    handleNetworkError();
}

void XMLHttpRequest::didFinishLoading(unsigned long identifier, double)
{
    WTF_LOG(Network, "XMLHttpRequest %p didFinishLoading(%lu)", this, identifier);

    if (m_error)
        return;

    if (m_state < HEADERS_RECEIVED)
        changeState(HEADERS_RECEIVED);

    if (m_decoder)
        m_responseText = m_responseText.concatenateWith(m_decoder->flush());

    if (m_responseStream)
        m_responseStream->finalize();

    clearVariablesForLoading();

    InspectorInstrumentation::didFinishXHRLoading(executionContext(), this, this, identifier, m_responseText, m_method, m_url, m_lastSendURL, m_lastSendLineNumber);

    // Prevent dropProtection releasing the last reference, and retain |this| until the end of this method.
    RefPtrWillBeRawPtr<XMLHttpRequest> protect(this);

    if (m_loader) {
        m_loader = nullptr;
        dropProtection();
    }

    changeState(DONE);
}

void XMLHttpRequest::didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent)
{
    WTF_LOG(Network, "XMLHttpRequest %p didSendData(%llu, %llu)", this, bytesSent, totalBytesToBeSent);

    if (!m_upload)
        return;

    if (m_uploadEventsAllowed)
        m_upload->dispatchProgressEvent(bytesSent, totalBytesToBeSent);

    if (bytesSent == totalBytesToBeSent && !m_uploadComplete) {
        m_uploadComplete = true;
        if (m_uploadEventsAllowed)
            m_upload->dispatchEventAndLoadEnd(EventTypeNames::load, true, bytesSent, totalBytesToBeSent);
    }
}

void XMLHttpRequest::didReceiveResponse(unsigned long identifier, const ResourceResponse& response)
{
    WTF_LOG(Network, "XMLHttpRequest %p didReceiveResponse(%lu)", this, identifier);

    m_response = response;
    if (!m_mimeTypeOverride.isEmpty()) {
        m_response.setHTTPHeaderField("Content-Type", m_mimeTypeOverride);
        m_responseEncoding = extractCharsetFromMediaType(m_mimeTypeOverride);
    }

    if (m_responseEncoding.isEmpty())
        m_responseEncoding = response.textEncodingName();
}

void XMLHttpRequest::didReceiveData(const char* data, int len)
{
    ASSERT(m_responseTypeCode != ResponseTypeBlob);

    if (m_error)
        return;

    if (m_state < HEADERS_RECEIVED)
        changeState(HEADERS_RECEIVED);

    bool useDecoder = m_responseTypeCode == ResponseTypeDefault || m_responseTypeCode == ResponseTypeText || m_responseTypeCode == ResponseTypeJSON || m_responseTypeCode == ResponseTypeDocument;

    if (useDecoder && !m_decoder) {
        if (m_responseTypeCode == ResponseTypeJSON) {
            m_decoder = TextResourceDecoder::create("application/json", "UTF-8");
        } else if (!m_responseEncoding.isEmpty()) {
            m_decoder = TextResourceDecoder::create("text/plain", m_responseEncoding);
        // allow TextResourceDecoder to look inside the m_response if it's XML or HTML
        } else if (responseIsXML()) {
            m_decoder = TextResourceDecoder::create("application/xml");
            // Don't stop on encoding errors, unlike it is done for other kinds
            // of XML resources. This matches the behavior of previous WebKit
            // versions, Firefox and Opera.
            m_decoder->useLenientXMLDecoding();
        } else if (equalIgnoringCase(responseMIMEType(), "text/html")) {
            m_decoder = TextResourceDecoder::create("text/html", "UTF-8");
        } else {
            m_decoder = TextResourceDecoder::create("text/plain", "UTF-8");
        }
    }

    if (!len)
        return;

    if (len == -1)
        len = strlen(data);

    if (useDecoder) {
        m_responseText = m_responseText.concatenateWith(m_decoder->decode(data, len));
    } else if (m_responseTypeCode == ResponseTypeArrayBuffer) {
        // Buffer binary data.
        if (!m_binaryResponseBuilder)
            m_binaryResponseBuilder = SharedBuffer::create();
        m_binaryResponseBuilder->append(data, len);
    } else if (m_responseTypeCode == ResponseTypeStream) {
        if (!m_responseStream)
            m_responseStream = Stream::create(executionContext(), responseMIMEType());
        m_responseStream->addData(data, len);
    }

    if (m_error)
        return;

    trackProgress(len);
}

void XMLHttpRequest::didDownloadData(int dataLength)
{
    ASSERT(m_responseTypeCode == ResponseTypeBlob);

    if (m_error)
        return;

    if (m_state < HEADERS_RECEIVED)
        changeState(HEADERS_RECEIVED);

    if (!dataLength)
        return;

    // readystatechange event handler may do something to put this XHR in error
    // state. We need to check m_error again here.
    if (m_error)
        return;

    m_downloadedBlobLength += dataLength;

    trackProgress(dataLength);
}

void XMLHttpRequest::handleDidTimeout()
{
    WTF_LOG(Network, "XMLHttpRequest %p handleDidTimeout()", this);

    // internalAbort() calls dropProtection(), which may release the last reference.
    RefPtrWillBeRawPtr<XMLHttpRequest> protect(this);

    // Response is cleared next, save needed progress event data.
    long long expectedLength = m_response.expectedContentLength();
    long long receivedLength = m_receivedLength;

    if (!internalAbort())
        return;

    handleDidFailGeneric();
    handleRequestError(TimeoutError, EventTypeNames::timeout, receivedLength, expectedLength);
}

void XMLHttpRequest::suspend()
{
    m_progressEventThrottle.suspend();
}

void XMLHttpRequest::resume()
{
    m_progressEventThrottle.resume();
}

void XMLHttpRequest::stop()
{
    internalAbort(DropProtectionAsync);
}

void XMLHttpRequest::contextDestroyed()
{
    ASSERT(!m_loader);
    ActiveDOMObject::contextDestroyed();
}

const AtomicString& XMLHttpRequest::interfaceName() const
{
    return EventTargetNames::XMLHttpRequest;
}

ExecutionContext* XMLHttpRequest::executionContext() const
{
    return ActiveDOMObject::executionContext();
}

void XMLHttpRequest::trace(Visitor* visitor)
{
    visitor->trace(m_responseBlob);
    visitor->trace(m_responseStream);
    visitor->trace(m_responseDocument);
    visitor->trace(m_progressEventThrottle);
    visitor->trace(m_upload);
    XMLHttpRequestEventTarget::trace(visitor);
}

} // namespace WebCore
