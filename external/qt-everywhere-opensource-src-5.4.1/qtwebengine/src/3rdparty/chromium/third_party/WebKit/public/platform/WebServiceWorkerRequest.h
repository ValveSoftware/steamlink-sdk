// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebServiceWorkerRequest_h
#define WebServiceWorkerRequest_h

#include "WebCommon.h"
#include "public/platform/WebPrivatePtr.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"

#if INSIDE_BLINK
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/text/StringHash.h"
#endif

namespace blink {

class WebServiceWorkerRequestPrivate;

// Represents a request of a fetch operation. FetchEvent dispatched by the
// browser contains this. The plan is for the Cache and fetch() API to also use
// it.
class BLINK_PLATFORM_EXPORT WebServiceWorkerRequest {
public:
    ~WebServiceWorkerRequest() { reset(); }
    WebServiceWorkerRequest();
    WebServiceWorkerRequest& operator=(const WebServiceWorkerRequest& other)
    {
        assign(other);
        return *this;
    }

    void reset();
    void assign(const WebServiceWorkerRequest&);

    void setURL(const WebURL&);
    WebURL url() const;

    void setMethod(const WebString&);
    WebString method() const;

    void setHeader(const WebString& key, const WebString& value);

#if INSIDE_BLINK
    const HashMap<String, String>& headers() const;
#endif

private:
    WebPrivatePtr<WebServiceWorkerRequestPrivate> m_private;
};

} // namespace blink

#endif // WebServiceWorkerRequest_h
