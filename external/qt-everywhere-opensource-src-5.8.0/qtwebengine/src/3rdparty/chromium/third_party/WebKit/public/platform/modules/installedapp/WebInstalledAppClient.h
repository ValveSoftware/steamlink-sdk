// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebInstalledAppClient_h
#define WebInstalledAppClient_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/installedapp/WebRelatedApplication.h"

namespace blink {

using AppInstalledCallbacks = WebCallbacks<const WebVector<WebRelatedApplication>&, void>;

class WebInstalledAppClient {
public:
    virtual ~WebInstalledAppClient() {}

    // Takes ownership of the AppInstalledCallbacks.
    virtual void getInstalledRelatedApps(const WebSecurityOrigin&, std::unique_ptr<AppInstalledCallbacks>) = 0;
};

} // namespace blink

#endif // WebInstalledAppClient_h
