// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebURLLoaderTestDelegate.h"

#include "public/platform/WebURLError.h"
#include "public/platform/WebURLLoader.h"
#include "public/platform/WebURLLoaderClient.h"
#include "public/platform/WebURLRequest.h"

namespace blink {

WebURLLoaderTestDelegate::WebURLLoaderTestDelegate()
{
}

WebURLLoaderTestDelegate::~WebURLLoaderTestDelegate()
{
}

void WebURLLoaderTestDelegate::didReceiveResponse(WebURLLoaderClient* originalClient, WebURLLoader* loader, const WebURLResponse& response)
{
    originalClient->didReceiveResponse(loader, response);
}

void WebURLLoaderTestDelegate::didReceiveData(WebURLLoaderClient* originalClient, WebURLLoader* loader, const char* data, int dataLength, int encodedDataLength)
{
    originalClient->didReceiveData(loader, data, dataLength, encodedDataLength);
}

void WebURLLoaderTestDelegate::didFail(WebURLLoaderClient* originalClient, WebURLLoader* loader, const WebURLError& error)
{
    originalClient->didFail(loader, error);
}

void WebURLLoaderTestDelegate::didFinishLoading(WebURLLoaderClient* originalClient, WebURLLoader* loader, double finishTime, int64_t totalEncodedDataLength)
{
    originalClient->didFinishLoading(loader, finishTime, totalEncodedDataLength);
}

} // namespace blink
