// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebURLLoaderMockFactory_h
#define WebURLLoaderMockFactory_h

#include "public/platform/WebCommon.h"
#include "public/platform/WebData.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURLLoaderTestDelegate.h"

#include <memory>

namespace blink {

class WebURL;
class WebURLResponse;
struct WebURLError;

class WebURLLoaderMockFactory {
public:
    static std::unique_ptr<WebURLLoaderMockFactory> create();

    virtual ~WebURLLoaderMockFactory() {}

    // Create a WebURLLoader that takes care of mocked requests.
    // Non-mocked request are forwarded to given loader which should not
    // be nullptr.
    virtual WebURLLoader* createURLLoader(WebURLLoader*) = 0;

    // Registers a response and the file to be served when the specified URL
    // is loaded. If no file is specified then the response content will be empty.
    virtual void registerURL(const WebURL&, const WebURLResponse&, const WebString& filePath) = 0;

    // Registers an error to be served when the specified URL is requested.
    virtual void registerErrorURL(const WebURL&, const WebURLResponse&, const WebURLError&) = 0;

    // Unregisters URLs so they are no longer mocked.
    virtual void unregisterURL(const WebURL&) = 0;
    virtual void unregisterAllURLs() = 0;

    // Causes all pending asynchronous requests to be served. When this method
    // returns all the pending requests have been processed.
    // Note: this may not work as expected if more requests could be made
    // asynchronously from different threads (e.g. when HTML parser thread
    // is being involved).
    // DO NOT USE THIS for Frame loading; always use methods defined in
    // FrameTestHelpers instead.
    virtual void serveAsynchronousRequests() = 0;

    // Set a delegate that allows callbacks for all WebURLLoaderClients to be intercepted.
    virtual void setLoaderDelegate(WebURLLoaderTestDelegate*) = 0;
};

} // namespace blink

#endif // WebURLLoaderMockFactory_h
