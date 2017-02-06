// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/presentation/presentation_connection_client.h"

#include "base/logging.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace content {

PresentationConnectionClient::PresentationConnectionClient(
    blink::mojom::PresentationSessionInfoPtr session_info)
    : url_(blink::WebString::fromUTF8(session_info->url)),
      id_(blink::WebString::fromUTF8(session_info->id)) {}

PresentationConnectionClient::PresentationConnectionClient(
    const mojo::String& url,
    const mojo::String& id)
    : url_(blink::WebString::fromUTF8(url)),
      id_(blink::WebString::fromUTF8(id)) {
}

PresentationConnectionClient::~PresentationConnectionClient() {
}

blink::WebString PresentationConnectionClient::getUrl() {
    return url_;
}

blink::WebString PresentationConnectionClient::getId() {
    return id_;
}

}  // namespace content
