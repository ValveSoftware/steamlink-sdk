// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPresentationConnectionClient_h
#define WebPresentationConnectionClient_h

#include "public/platform/WebString.h"

namespace blink {

enum class WebPresentationConnectionState {
    Connected = 0,
    Closed,
    Terminated,
};

enum class WebPresentationConnectionCloseReason {
    Error = 0,
    Closed,
    WentAway
};

// The implementation the embedder has to provide for the Presentation API to work.
class WebPresentationConnectionClient {
public:
    virtual ~WebPresentationConnectionClient() { }

    virtual WebString getId() = 0;
    virtual WebString getUrl() = 0;
};

} // namespace blink

#endif // WebPresentationConnectionClient_h
