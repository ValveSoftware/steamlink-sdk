// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Request_h
#define Request_h

#include "bindings/v8/Dictionary.h"
#include "bindings/v8/ScriptWrappable.h"
#include "modules/serviceworkers/HeaderMap.h"
#include "platform/weborigin/KURL.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace blink { class WebServiceWorkerRequest; }

namespace WebCore {

struct RequestInit;
class ResourceRequest;

class Request FINAL : public ScriptWrappable, public RefCounted<Request> {
public:
    static PassRefPtr<Request> create();
    static PassRefPtr<Request> create(const Dictionary&);
    static PassRefPtr<Request> create(const blink::WebServiceWorkerRequest&);
    ~Request() { };

    void setURL(const String& value);
    void setMethod(const String& value);

    String url() const { return m_url.string(); }
    String method() const { return m_method; }
    String origin() const;
    PassRefPtr<HeaderMap> headers() const { return m_headers; }

    PassOwnPtr<ResourceRequest> createResourceRequest() const;

private:
    explicit Request(const RequestInit&);
    explicit Request(const blink::WebServiceWorkerRequest&);
    KURL m_url;
    String m_method;
    RefPtr<HeaderMap> m_headers;
};

} // namespace WebCore

#endif // Request_h
