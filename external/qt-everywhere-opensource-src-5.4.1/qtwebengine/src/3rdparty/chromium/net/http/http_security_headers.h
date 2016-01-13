// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_SECURITY_HEADERS_H_
#define NET_HTTP_HTTP_SECURITY_HEADERS_H_

#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/base/hash_value.h"
#include "net/base/net_export.h"

namespace net {

const int64 kMaxHSTSAgeSecs = 86400 * 365;  // 1 year

// Parses |value| as a Strict-Transport-Security header value. If successful,
// returns true and sets |*max_age| and |*include_subdomains|.
// Otherwise returns false and leaves the output parameters unchanged.
//
// value is the right-hand side of:
//
// "Strict-Transport-Security" ":"
//     [ directive ]  *( ";" [ directive ] )
bool NET_EXPORT_PRIVATE ParseHSTSHeader(const std::string& value,
                                        base::TimeDelta* max_age,
                                        bool* include_subdomains);

// Parses |value| as a Public-Key-Pins header value. If successful, returns
// true and populates the |*max_age|, |*include_subdomains|, and |*hashes|
// values. Otherwise returns false and leaves the output parameters
// unchanged.
//
// value is the right-hand side of:
//
// "Public-Key-Pins" ":"
//     "max-age" "=" delta-seconds ";"
//     "pin-" algo "=" base64 [ ";" ... ]
//     [ ";" "includeSubdomains" ]
//
// For this function to return true, the key hashes specified by the HPKP
// header must pass two additional checks. There MUST be at least one key
// hash which matches the SSL certificate chain of the current site (as
// specified by the chain_hashes) parameter. In addition, there MUST be at
// least one key hash which does NOT match the site's SSL certificate chain
// (this is the "backup pin").
bool NET_EXPORT_PRIVATE ParseHPKPHeader(const std::string& value,
                                        const HashValueVector& chain_hashes,
                                        base::TimeDelta* max_age,
                                        bool* include_subdomains,
                                        HashValueVector* hashes);

}  // namespace net

#endif  // NET_HTTP_HTTP_SECURITY_HEADERS_H_
