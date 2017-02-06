// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Request_h
#define Request_h

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/modules/v8/RequestOrUSVString.h"
#include "modules/ModulesExport.h"
#include "modules/fetch/Body.h"
#include "modules/fetch/FetchRequestData.h"
#include "modules/fetch/Headers.h"
#include "platform/heap/Handle.h"
#include "platform/network/EncodedFormData.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebURLRequest.h"
#include "wtf/text/WTFString.h"

namespace blink {

class BodyStreamBuffer;
class EncodedFormData;
class RequestInit;
class WebServiceWorkerRequest;

using RequestInfo = RequestOrUSVString;

class MODULES_EXPORT Request final : public Body {
    DEFINE_WRAPPERTYPEINFO();
    WTF_MAKE_NONCOPYABLE(Request);
public:
    // These "create" function must be called with entering an appropriate
    // V8 context.
    // From Request.idl:
    static Request* create(ScriptState*, const RequestInfo&, const Dictionary&, ExceptionState&);

    static Request* create(ScriptState*, const String&, ExceptionState&);
    static Request* create(ScriptState*, const String&, const Dictionary&, ExceptionState&);
    static Request* create(ScriptState*, Request*, ExceptionState&);
    static Request* create(ScriptState*, Request*, const Dictionary&, ExceptionState&);
    static Request* create(ScriptState*, FetchRequestData*);
    static Request* create(ScriptState*, const WebServiceWorkerRequest&);

    // From Request.idl:
    String method() const;
    KURL url() const;
    Headers* getHeaders() const { return m_headers; }
    String context() const;
    String referrer() const;
    String referrerPolicy() const;
    String mode() const;
    String credentials() const;
    String redirect() const;
    String integrity() const;

    // From Request.idl:
    // This function must be called with entering an appropriate V8 context.
    Request* clone(ScriptState*, ExceptionState&);

    FetchRequestData* passRequestData(ScriptState*);
    void populateWebServiceWorkerRequest(WebServiceWorkerRequest&) const;
    bool hasBody() const;
    BodyStreamBuffer* bodyBuffer() override { return m_request->buffer(); }
    const BodyStreamBuffer* bodyBuffer() const override { return m_request->buffer(); }
    PassRefPtr<EncodedFormData> attachedCredential() const { return m_request->attachedCredential(); }

    DECLARE_VIRTUAL_TRACE();

private:
    Request(ScriptState*, FetchRequestData*, Headers*);
    Request(ScriptState*, FetchRequestData*);
    Request(ScriptState*, const WebServiceWorkerRequest&);

    const FetchRequestData* request() const { return m_request; }
    static Request* createRequestWithRequestOrString(ScriptState*, Request*, const String&, RequestInit&, ExceptionState&);

    String mimeType() const override;
    void refreshBody(ScriptState*);

    const Member<FetchRequestData> m_request;
    const Member<Headers> m_headers;
};

} // namespace blink

#endif // Request_h
