// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebDocumentSubresourceFilter_h
#define WebDocumentSubresourceFilter_h

#include "public/platform/WebURLRequest.h"

namespace blink {

class WebURL;

class WebDocumentSubresourceFilter {
public:
    virtual ~WebDocumentSubresourceFilter() {}
    virtual bool allowLoad(const WebURL& resourceUrl, WebURLRequest::RequestContext) = 0;
};

} // namespace blink

#endif // WebDocumentSubresourceFilter_h
