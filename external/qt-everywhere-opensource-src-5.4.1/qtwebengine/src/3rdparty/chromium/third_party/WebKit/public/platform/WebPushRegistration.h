// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPushRegistration_h
#define WebPushRegistration_h

#include "WebString.h"

namespace blink {

struct WebPushRegistration {
    WebPushRegistration(const WebString& endpoint, const WebString& registrationId)
        : endpoint(endpoint)
        , registrationId(registrationId)
    {
    }

    WebString endpoint;
    WebString registrationId;
};

} // namespace blink

#endif // WebPushRegistration_h
