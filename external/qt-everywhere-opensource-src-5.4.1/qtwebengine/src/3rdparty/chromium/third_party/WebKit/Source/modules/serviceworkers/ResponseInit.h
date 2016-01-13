// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ResponseInit_h
#define ResponseInit_h

#include "bindings/v8/Dictionary.h"
#include "modules/serviceworkers/HeaderMap.h"
#include "wtf/RefPtr.h"

namespace WebCore {

struct ResponseInit {
    explicit ResponseInit(const Dictionary& options)
        : status(200)
        , statusText("OK")
    {
        options.get("status", status);
        // FIXME: Spec uses ByteString for statusText. http://crbug.com/347426
        options.get("statusText", statusText);
        options.get("headers", headers);
    }

    unsigned short status;
    String statusText;
    RefPtr<HeaderMap> headers;
};

}

#endif // ResponseInit_h
