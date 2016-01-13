// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_NET_INFO_HELPER_H_
#define CONTENT_PUBLIC_RENDERER_NET_INFO_HELPER_H_

#include "net/base/network_change_notifier.h"
#include "third_party/WebKit/public/platform/WebConnectionType.h"

namespace content {

blink::WebConnectionType NetConnectionTypeToWebConnectionType(
    net::NetworkChangeNotifier::ConnectionType type);

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_NET_INFO_HELPER_H_
