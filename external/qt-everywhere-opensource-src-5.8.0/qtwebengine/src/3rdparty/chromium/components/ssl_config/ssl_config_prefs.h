// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SSL_CONFIG_SSL_CONFIG_PREFS_H_
#define COMPONENTS_SSL_CONFIG_SSL_CONFIG_PREFS_H_

namespace ssl_config {
namespace prefs {

extern const char kCertRevocationCheckingEnabled[];
extern const char kCertRevocationCheckingRequiredLocalAnchors[];
extern const char kSSLVersionMin[];
extern const char kSSLVersionMax[];
extern const char kCipherSuiteBlacklist[];
extern const char kDHEEnabled[];

}  // namespace prefs
}  // namespace ssl_config

#endif  // COMPONENTS_SSL_CONFIG_SSL_CONFIG_PREFS_H_
