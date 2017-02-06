// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Response_h
#define Response_h

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/ModulesExport.h"
#include "modules/fetch/Body.h"
#include "modules/fetch/BodyStreamBuffer.h"
#include "modules/fetch/FetchResponseData.h"
#include "modules/fetch/Headers.h"
#include "platform/blob/BlobData.h"
#include "platform/heap/Handle.h"

namespace blink {

class Blob;
class DOMArrayBuffer;
class ExceptionState;
class FetchDataConsumerHandle;
class ResponseInit;
class ScriptState;
class WebServiceWorkerResponse;

class MODULES_EXPORT Response final : public Body {
    DEFINE_WRAPPERTYPEINFO();
    WTF_MAKE_NONCOPYABLE(Response);
public:
    // These "create" function which takes a ScriptState* must be called with
    // entering an appropriate V8 context.
    // From Response.idl:
    static Response* create(ScriptState*, ExceptionState&);
    static Response* create(ScriptState*, ScriptValue body, const Dictionary&, ExceptionState&);

    static Response* create(ScriptState*, BodyStreamBuffer*, const String& contentType, const ResponseInit&, ExceptionState&);
    static Response* create(ExecutionContext*, FetchResponseData*);
    static Response* create(ScriptState*, const WebServiceWorkerResponse&);

    static Response* createClone(const Response&);

    static Response* error(ExecutionContext*);
    static Response* redirect(ExecutionContext*, const String& url, unsigned short status, ExceptionState&);

    const FetchResponseData* response() const { return m_response; }

    // From Response.idl:
    String type() const;
    String url() const;
    unsigned short status() const;
    bool ok() const;
    String statusText() const;
    Headers* headers() const;

    // From Response.idl:
    // This function must be called with entering an appropriate V8 context.
    Response* clone(ScriptState*, ExceptionState&);

    // ActiveScriptWrappable
    bool hasPendingActivity() const final;

    // Does not call response.setBlobDataHandle().
    void populateWebServiceWorkerResponse(WebServiceWorkerResponse& /* response */);

    bool hasBody() const;
    BodyStreamBuffer* bodyBuffer() override { return m_response->buffer(); }
    const BodyStreamBuffer* bodyBuffer() const override { return m_response->buffer(); }
    BodyStreamBuffer* internalBodyBuffer() { return m_response->internalBuffer(); }
    const BodyStreamBuffer* internalBodyBuffer() const { return m_response->internalBuffer(); }
    bool bodyUsed() override;

    String mimeType() const override;
    String internalMIMEType() const;

    DECLARE_VIRTUAL_TRACE();

private:
    explicit Response(ExecutionContext*);
    Response(ExecutionContext*, FetchResponseData*);
    Response(ExecutionContext*, FetchResponseData*, Headers*);

    void installBody();
    void refreshBody(ScriptState*);

    const Member<FetchResponseData> m_response;
    const Member<Headers> m_headers;
};

} // namespace blink

#endif // Response_h
