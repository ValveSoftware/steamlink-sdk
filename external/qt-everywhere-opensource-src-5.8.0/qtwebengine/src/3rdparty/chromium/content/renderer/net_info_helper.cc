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
      return blink::WebConnectionTypeUnknown;
    case net::NetworkChangeNotifier::CONNECTION_ETHERNET:
      return blink::WebConnectionTypeEthernet;
    case net::NetworkChangeNotifier::CONNECTION_WIFI:
      return blink::WebConnectionTypeWifi;
    case net::NetworkChangeNotifier::CONNECTION_NONE:
      return blink::WebConnectionTypeNone;
    case net::NetworkChangeNotifier::CONNECTION_2G:
      return blink::WebConnectionTypeCellular2G;
    case net::NetworkChangeNotifier::CONNECTION_3G:
      return blink::WebConnectionTypeCellular3G;
    case net::NetworkChangeNotifier::CONNECTION_4G:
      return blink::WebConnectionTypeCellular4G;
    case net::NetworkChangeNotifier::CONNECTION_BLUETOOTH:
      return blink::WebConnectionTypeBluetooth;
  }

  NOTREACHED();
  return blink::WebConnectionTypeNone;
}

}  // namespace content
