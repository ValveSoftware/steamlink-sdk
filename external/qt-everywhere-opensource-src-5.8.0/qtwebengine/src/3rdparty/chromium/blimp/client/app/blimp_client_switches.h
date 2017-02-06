// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the command-line switches used by //blimp.

#ifndef BLIMP_CLIENT_APP_BLIMP_CLIENT_SWITCHES_H_
#define BLIMP_CLIENT_APP_BLIMP_CLIENT_SWITCHES_H_

namespace blimp {
namespace switches {

// The path to the engine's PEM-encoded X509 certificate.
// If specified, SSL connected Engines must supply this certificate
// for the connection to be valid.
// e.g.:
// --engine-cert-path=/home/blimp/certs/cert.pem
extern const char kEngineCertPath[];

// Specifies the engine's IP address. Must be used in conjunction with
// --engine-port and --engine-transport.
extern const char kEngineIP[];

// Specifies the engine's listening port (1-65535).
// Must be used in conjunction with --engine-ip and --engine-transport.
extern const char kEnginePort[];

// Specifies the transport used to communicate with the engine.
// Can be "tcp" or "ssl".
// Must be used in conjunction with --engine-ip and --engine-port.
extern const char kEngineTransport[];

// Enables downloading the complete page from the engine.
extern const char kDownloadWholeDocument[];

}  // namespace switches
}  // namespace blimp

#endif  // BLIMP_CLIENT_APP_BLIMP_CLIENT_SWITCHES_H_
