// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NETWORK_SESSION_CONFIGURATOR_SWITCHES_H_
#define COMPONENTS_NETWORK_SESSION_CONFIGURATOR_SWITCHES_H_

namespace switches {

extern const char kDisableHttp2[];
extern const char kDisableQuicPortSelection[];
extern const char kDisableQuic[];
extern const char kEnableQuicPortSelection[];
extern const char kEnableQuic[];
extern const char kIgnoreCertificateErrors[];
extern const char kIgnoreUrlFetcherCertRequests[];
extern const char kOriginToForceQuicOn[];
extern const char kQuicConnectionOptions[];
extern const char kQuicHostWhitelist[];
extern const char kQuicMaxPacketLength[];
extern const char kQuicVersion[];
extern const char kTestingFixedHttpPort[];
extern const char kTestingFixedHttpsPort[];

}  // namespace switches

#endif  // COMPONENTS_NETWORK_SESSION_CONFIGURATOR_SWITCHES_H_
