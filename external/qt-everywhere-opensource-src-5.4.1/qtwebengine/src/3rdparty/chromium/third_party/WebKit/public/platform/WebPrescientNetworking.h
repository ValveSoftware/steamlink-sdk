/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebPrescientNetworking_h
#define WebPrescientNetworking_h

#include "WebCommon.h"
#include "WebString.h"

namespace blink {

class WebURL;

// FIXME: Passing preconnect motivation to the platform is layering vioration.
// However, this is done temporarily to perform a Finch field trial to enable
// preconnect in different triggers one at a time. This enum can be safely
// removed after the field trial is done.
enum WebPreconnectMotivation {
    WebPreconnectMotivationLinkMouseDown,
    WebPreconnectMotivationLinkMouseOver,
    WebPreconnectMotivationLinkTapUnconfirmed,
    WebPreconnectMotivationLinkTapDown,
};

class WebPrescientNetworking {
public:
    virtual ~WebPrescientNetworking() { }

    // When a page navigation is speculated, DNS prefetch is triggered to hide
    // the host resolution latency.
    virtual void prefetchDNS(const WebString& hostname) { }

    // When a page navigation is speculated, preconnect is triggered to hide
    // session initialization latency to the server providing the page resource.
    virtual void preconnect(const WebURL&, WebPreconnectMotivation) { }
};

} // namespace blink

#endif // WebPrescientNetworking_h
