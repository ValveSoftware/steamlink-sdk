// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/net_info_helper.h"

namespace content {

blink::WebConnectionType
NetConnectionTypeToWebConnectionType(
    net::NetworkChangeNotifier::ConnectionType net_type) {
  switch (net_type) {
    case net::NetworkChangeNotifier::CONNECTION_UNKNOWN:
      return blink::ConnectionTypeOther;
    case net::NetworkChangeNotifier::CONNECTION_ETHERNET:
      return blink::ConnectionTypeEthernet;
    case net::NetworkChangeNotifier::CONNECTION_WIFI:
      return blink::ConnectionTypeWifi;
    case net::NetworkChangeNotifier::CONNECTION_NONE:
      return blink::ConnectionTypeNone;
    case net::NetworkChangeNotifier::CONNECTION_2G:
    case net::NetworkChangeNotifier::CONNECTION_3G:
    case net::NetworkChangeNotifier::CONNECTION_4G:
      return blink::ConnectionTypeCellular;
    case net::NetworkChangeNotifier::CONNECTION_BLUETOOTH:
      return blink::ConnectionTypeBluetooth;
  }

  NOTREACHED();
  return blink::ConnectionTypeNone;
}

}  // namespace content
