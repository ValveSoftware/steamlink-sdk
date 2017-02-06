// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ssl_config/ssl_config_switches.h"

namespace switches {

// Specifies the maximum SSL/TLS version ("tls1", "tls1.1", or "tls1.2").
const char kSSLVersionMax[] = "ssl-version-max";

// Specifies the minimum SSL/TLS version ("tls1", "tls1.1", or "tls1.2").
const char kSSLVersionMin[] = "ssl-version-min";

// These values aren't switches, but rather the values that kSSLVersionMax and
// kSSLVersionMin can have.
const char kSSLVersionTLSv1[] = "tls1";
const char kSSLVersionTLSv11[] = "tls1.1";
const char kSSLVersionTLSv12[] = "tls1.2";

}  // namespace switches
