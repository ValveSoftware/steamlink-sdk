// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebURLLoaderTestDelegate_h
#define WebURLLoaderTestDelegate_h

#include "public/platform/WebCommon.h"

namespace blink {

class WebURLLoader;
class WebURLResponse;
class WebURLLoaderClient;
struct WebURLError;

// Use with WebUnitTestSupport::setLoaderDelegate to intercept calls to a
// WebURLLoaderClient for controlling network responses in a test. Default
// implementations of all methods just call the original method on the
// WebURLLoaderClient.
class BLINK_PLATFORM_EXPORT WebURLLoaderTestDelegate {
public:
    WebURLLoaderTestDelegate();
    virtual ~WebURLLoaderTestDelegate();

    virtual void didReceiveResponse(WebURLLoaderClient* originalClient, WebURLLoader*, const WebURLResponse&);
    virtual void didReceiveData(WebURLLoaderClient* originalClient, WebURLLoader*, const char* data, int dataLength, int encodedDataLength);
    virtual void didFail(WebURLLoaderClient* originalClient, WebURLLoader*, const WebURLError&);
    virtual void didFinishLoading(WebURLLoaderClient* originalClient, WebURLLoader*, double finishTime, int64_t totalEncodedDataLength);
};

} // namespace blink

#endif
