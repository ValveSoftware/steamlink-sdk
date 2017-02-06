// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_hints/renderer/prescient_networking_dispatcher.h"

#include "base/logging.h"

namespace network_hints {

PrescientNetworkingDispatcher::PrescientNetworkingDispatcher() {
}

PrescientNetworkingDispatcher::~PrescientNetworkingDispatcher() {
}

void PrescientNetworkingDispatcher::prefetchDNS(
    const blink::WebString& hostname) {
  VLOG(2) << "Prefetch DNS: " << hostname.utf8();
  if (hostname.isEmpty())
    return;

  std::string hostname_utf8 = hostname.utf8();
  dns_prefetch_.Resolve(hostname_utf8.data(), hostname_utf8.length());
}

void PrescientNetworkingDispatcher::preconnect(const blink::WebURL& url,
                                               bool allow_credentials) {
  VLOG(2) << "Preconnect: " << url.string().utf8();
  preconnect_.Preconnect(url, allow_credentials);
}

}  // namespace network_hints
