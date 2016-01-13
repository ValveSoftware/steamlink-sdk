// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RequestInit_h
#define RequestInit_h

#include "bindings/v8/Dictionary.h"
#include "modules/serviceworkers/HeaderMap.h"
#include "wtf/RefPtr.h"

namespace WebCore {

struct RequestInit {
    explicit RequestInit(const Dictionary& options)
        : method("GET")
    {
        options.get("url", url);
        // FIXME: Spec uses ByteString for method. http://crbug.com/347426
        options.get("method", method);
        options.get("headers", headers);
    }

    String url;
    String method;
    RefPtr<HeaderMap> headers;
};

}

#endif // RequestInit_h
