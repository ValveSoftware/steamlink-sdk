// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_session_configurator/switches.h"

namespace switches {

// Disables the HTTP/2 and SPDY/3.1 protocols.
const char kDisableHttp2[] = "disable-http2";

// Disables the QUIC protocol.
const char kDisableQuic[] = "disable-quic";

// Disable use of Chromium's port selection for the ephemeral port via bind().
// This only has an effect if QUIC protocol is enabled.
const char kDisableQuicPortSelection[] = "disable-quic-port-selection";

// Enables the QUIC protocol.  This is a temporary testing flag.
const char kEnableQuic[] = "enable-quic";

// Enable use of Chromium's port selection for the ephemeral port via bind().
// This only has an effect if the QUIC protocol is enabled.
const char kEnableQuicPortSelection[] = "enable-quic-port-selection";

// Ignores certificate-related errors.
const char kIgnoreCertificateErrors[] = "ignore-certificate-errors";

// Causes net::URLFetchers to ignore requests for SSL client certificates,
// causing them to attempt an unauthenticated SSL/TLS session. This is intended
// for use when testing various service URLs (eg: kPromoServerURL, kSbURLPrefix,
// kSyncServiceURL, etc)
const char kIgnoreUrlFetcherCertRequests[] = "ignore-urlfetcher-cert-requests";

// Specifies a comma separated list of host-port pairs to force use of QUIC on.
const char kOriginToForceQuicOn[] = "origin-to-force-quic-on";

// Specifies a comma separated list of QUIC connection options to send to
// the server.
const char kQuicConnectionOptions[] = "quic-connection-options";

// Specifies a comma separated list of hosts to whitelist QUIC for.
const char kQuicHostWhitelist[] = "quic-host-whitelist";

// Specifies the maximum length for a QUIC packet.
const char kQuicMaxPacketLength[] = "quic-max-packet-length";

// Specifies the version of QUIC to use.
const char kQuicVersion[] = "quic-version";

// Allows for forcing socket connections to http/https to use fixed ports.
const char kTestingFixedHttpPort[] = "testing-fixed-http-port";
const char kTestingFixedHttpsPort[] = "testing-fixed-https-port";

}  // namespace switches
