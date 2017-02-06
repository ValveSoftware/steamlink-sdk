// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/network/NetworkUtils.h"

#include "net/base/ip_address.h"
#include "wtf/text/StringUTF8Adaptor.h"
#include "wtf/text/WTFString.h"

namespace blink {

namespace NetworkUtils {

bool isReservedIPAddress(const String& host)
{
    net::IPAddress address;
    StringUTF8Adaptor utf8(host);
    if (!net::ParseURLHostnameToAddress(utf8.asStringPiece(), &address))
        return false;
    return address.IsReserved();
}

} // NetworkUtils

} // namespace blink
