// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/app/blimp_client_switches.h"

namespace blimp {
namespace switches {

const char kEngineCertPath[] = "engine-cert-path";

// Specifies the engine's IP address. Must be used in conjunction with
// --engine-port and --engine-transport.
// This is the same command line flag as ENGINE_IP in
// blimp/client/app/android/java/src/org/chromium/blimp/BlimpClientSwitches.java
const char kEngineIP[] = "engine-ip";

const char kEnginePort[] = "engine-port";

const char kEngineTransport[] = "engine-transport";

const char kDownloadWholeDocument[] = "download-whole-document";

}  // namespace switches
}  // namespace blimp
