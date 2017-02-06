// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ssl_config/ssl_config_prefs.h"

namespace ssl_config {
namespace prefs {

// Prefs for SSLConfigServicePref.
const char kCertRevocationCheckingEnabled[] = "ssl.rev_checking.enabled";
const char kCertRevocationCheckingRequiredLocalAnchors[] =
    "ssl.rev_checking.required_for_local_anchors";
const char kSSLVersionMin[] = "ssl.version_min";
const char kSSLVersionMax[] = "ssl.version_max";
const char kCipherSuiteBlacklist[] = "ssl.cipher_suites.blacklist";
const char kDHEEnabled[] = "ssl.dhe_enabled";

}  // namespace prefs
}  // namespace ssl_config
