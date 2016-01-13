/*
 *  Copyright (C) 2003, 2006, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2005, 2006 Alexey Proskuryakov <ap@nypop.com>
 *  Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef XMLHttpRequest_h
#define XMLHttpRequest_h

#include "bindings/v8/ScriptString.h"
#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/events/EventListener.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "core/xml/XMLHttpRequestEventTarget.h"
#include "core/xml/XMLHttpRequestProgressEventThrottle.h"
#include "platform/AsyncMethodRunner.h"
#include "platform/heap/Handle.h"
#include "platform/network/FormData.h"
#include "platform/network/ResourceResponse.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/OwnPtr.h"
#include "wtf/text/AtomicStringHash.h"
#include "wtf/text/StringBuilder.h"

namespace WebCore {

class Blob;
class DOMFormData;
class Document;
class ExceptionState;
class ResourceRequest;
class SecurityOrigin;
class SharedBuffer;
class Stream;
class TextResourceDecoder;
class ThreadableLoader;

typedef int ExceptionCode;

class XMLHttpRequest FINAL
    : public RefCountedWillBeRefCountedGarbageCollected<XMLHttpRequest>
    , public ScriptWrappable
    , public XMLHttpRequestEventTarget
    , private ThreadableLoaderClient
    , public ActiveDOMObject {
    WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED;
    REFCOUNTED_EVENT_TARGET(XMLHttpRequest);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(XMLHttpRequest);
public:
    static PassRefPtrWillBeRawPtr<XMLHttpRequest> create(ExecutionContext*, PassRefPtr<SecurityOrigin> = nullptr);
    virtual ~XMLHttpRequest();

    // These exact numeric values are important because JS expects them.
    enum State {
        UNSENT = 0,
        OPENED = 1,
        HEADERS_RECEIVED = 2,
        LOADING = 3,
        DONE = 4
    };

    enum ResponseTypeCode {
        ResponseTypeDefault,
        ResponseTypeText,
        ResponseTypeJSON,
        ResponseTypeDocument,
        ResponseTypeBlob,
        ResponseTypeArrayBuffer,
        ResponseTypeStream
    };

    enum DropProtection {
        DropProtectionSync,
        DropProtectionAsync,
    };

    virtual void contextDestroyed() OVERRIDE;
    virtual void suspend() OVERRIDE;
    virtual void resume() OVERRIDE;
    virtual void stop() OVERRIDE;

    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ExecutionContext* executionContext() const OVERRIDE;

    const KURL& url() const { return m_url; }
    String statusText() const;
    int status() const;
    State readyState() const;
    bool withCredentials() const { return m_includeCredentials; }
    void setWithCredentials(bool, ExceptionState&);
    void open(const AtomicString& method, const KURL&, ExceptionState&);
    void open(const AtomicString& method, const KURL&, bool async, ExceptionState&);
    void open(const AtomicString& method, const KURL&, bool async, const String& user, ExceptionState&);
    void open(const AtomicString& method, const KURL&, bool async, const String& user, const String& password, ExceptionState&);
    void send(ExceptionState&);
    void send(Document*, ExceptionState&);
    void send(const String&, ExceptionState&);
    void send(Blob*, ExceptionState&);
    void send(DOMFormData*, ExceptionState&);
    void send(ArrayBuffer*, ExceptionState&);
    void send(ArrayBufferView*, ExceptionState&);
    void abort();
    void setRequestHeader(const AtomicString& name, const AtomicString& value, ExceptionState&);
    void overrideMimeType(const AtomicString& override);
    String getAllResponseHeaders() const;
    const AtomicString& getResponseHeader(const AtomicString&) const;
    ScriptString responseText(ExceptionState&);
    ScriptString responseJSONSource();
    Document* responseXML(ExceptionState&);
    Blob* responseBlob();
    Stream* responseStream();
    unsigned long timeout() const { return m_timeoutMilliseconds; }
    void setTimeout(unsigned long timeout, ExceptionState&);

    void sendForInspectorXHRReplay(PassRefPtr<FormData>, ExceptionState&);

    // Expose HTTP validation methods for other untrusted requests.
    static bool isAllowedHTTPMethod(const String&);
    static AtomicString uppercaseKnownHTTPMethod(const AtomicString&);
    static bool isAllowedHTTPHeader(const String&);

    void setResponseType(const String&, ExceptionState&);
    String responseType();
    ResponseTypeCode responseTypeCode() const { return m_responseTypeCode; }

    String responseURL();

    // response attribute has custom getter.
    ArrayBuffer* responseArrayBuffer();

    void setLastSendLineNumber(unsigned lineNumber) { m_lastSendLineNumber = lineNumber; }
    void setLastSendURL(const String& url) { m_lastSendURL = url; }

    XMLHttpRequestUpload* upload();

    DEFINE_ATTRIBUTE_EVENT_LISTENER(readystatechange);

    virtual void trace(Visitor*) OVERRIDE;

private:
    XMLHttpRequest(ExecutionContext*, PassRefPtr<SecurityOrigin>);

    Document* document() const;
    SecurityOrigin* securityOrigin() const;

    virtual void didSendData(unsigned long long bytesSent, unsigned long long totalBytesToBeSent) OVERRIDE;
    virtual void didReceiveResponse(unsigned long identifier, const ResourceResponse&) OVERRIDE;
    virtual void didReceiveData(const char* data, int dataLength) OVERRIDE;
    // When responseType is set to "blob", didDownloadData() is called instead
    // of didReceiveData().
    virtual void didDownloadData(int dataLength) OVERRIDE;
    virtual void didFinishLoading(unsigned long identifier, double finishTime) OVERRIDE;
    virtual void didFail(const ResourceError&) OVERRIDE;
    virtual void didFailRedirectCheck() OVERRIDE;

    AtomicString responseMIMEType() const;
    bool responseIsXML() const;

    bool areMethodAndURLValidForSend();

    bool initSend(ExceptionState&);
    void sendBytesData(const void*, size_t, ExceptionState&);

    const AtomicString& getRequestHeader(const AtomicString& name) const;
    void setRequestHeaderInternal(const AtomicString& name, const AtomicString& value);

    void trackProgress(int dataLength);
    // Changes m_state and dispatches a readyStateChange event if new m_state
    // value is different from last one.
    void changeState(State newState);
    void dispatchReadyStateChangeEvent();

    void dropProtectionSoon();
    void dropProtection();
    // Clears variables used only while the resource is being loaded.
    void clearVariablesForLoading();
    // Returns false iff reentry happened and a new load is started.
    bool internalAbort(DropProtection = DropProtectionSync);
    // Clears variables holding response header and body data.
    void clearResponse();
    void clearRequest();

    void createRequest(PassRefPtr<FormData>, ExceptionState&);

    // Dispatches a response ProgressEvent.
    void dispatchProgressEvent(const AtomicString&, long long, long long);
    // Dispatches a response ProgressEvent using values sampled from
    // m_receivedLength and m_response.
    void dispatchProgressEventFromSnapshot(const AtomicString&);

    // Does clean up common for all kind of didFail() call.
    void handleDidFailGeneric();
    // Handles didFail() call not caused by cancellation or timeout.
    void handleNetworkError();
    // Handles didFail() call triggered by m_loader->cancel().
    void handleDidCancel();
    // Handles didFail() call for timeout.
    void handleDidTimeout();

    void handleRequestError(ExceptionCode, const AtomicString&, long long, long long);

    OwnPtrWillBeMember<XMLHttpRequestUpload> m_upload;

    KURL m_url;
    AtomicString m_method;
    HTTPHeaderMap m_requestHeaders;
    AtomicString m_mimeTypeOverride;
    bool m_async;
    bool m_includeCredentials;
    unsigned long m_timeoutMilliseconds;
    RefPtrWillBeMember<Blob> m_responseBlob;
    RefPtrWillBeMember<Stream> m_responseStream;

    RefPtr<ThreadableLoader> m_loader;
    State m_state;

    ResourceResponse m_response;
    String m_responseEncoding;

    OwnPtr<TextResourceDecoder> m_decoder;

    ScriptString m_responseText;
    // Used to skip m_responseDocument creation if it's done previously. We need
    // this separate flag since m_responseDocument can be 0 for some cases.
    bool m_createdDocument;
    RefPtrWillBeMember<Document> m_responseDocument;

    RefPtr<SharedBuffer> m_binaryResponseBuilder;
    long long m_downloadedBlobLength;

    RefPtr<ArrayBuffer> m_responseArrayBuffer;

    bool m_error;

    bool m_uploadEventsAllowed;
    bool m_uploadComplete;

    bool m_sameOriginRequest;

    // Used for onprogress tracking
    long long m_receivedLength;

    unsigned m_lastSendLineNumber;
    String m_lastSendURL;
    // An exception to throw in synchronous mode. It's set when failure
    // notification is received from m_loader and thrown at the end of send() if
    // any.
    ExceptionCode m_exceptionCode;

    XMLHttpRequestProgressEventThrottle m_progressEventThrottle;

    // An enum corresponding to the allowed string values for the responseType attribute.
    ResponseTypeCode m_responseTypeCode;
    AsyncMethodRunner<XMLHttpRequest> m_dropProtectionRunner;
    RefPtr<SecurityOrigin> m_securityOrigin;
};

} // namespace WebCore

#endif // XMLHttpRequest_h
