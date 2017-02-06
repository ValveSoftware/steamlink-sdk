// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebFederatedCredential_h
#define WebFederatedCredential_h

#include "public/platform/WebCommon.h"
#include "public/platform/WebCredential.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/WebString.h"

namespace blink {

class WebFederatedCredential : public WebCredential {
public:
    BLINK_PLATFORM_EXPORT WebFederatedCredential(const WebString& id, const WebSecurityOrigin& federation, const WebString& name, const WebURL& iconURL);

    BLINK_PLATFORM_EXPORT void assign(const WebFederatedCredential&);
    BLINK_PLATFORM_EXPORT WebSecurityOrigin provider() const;

#if INSIDE_BLINK
    BLINK_PLATFORM_EXPORT WebFederatedCredential(PlatformCredential*);
    BLINK_PLATFORM_EXPORT WebFederatedCredential& operator=(PlatformCredential*);
#endif
};

} // namespace blink

#endif // WebFederatedCredential_h


