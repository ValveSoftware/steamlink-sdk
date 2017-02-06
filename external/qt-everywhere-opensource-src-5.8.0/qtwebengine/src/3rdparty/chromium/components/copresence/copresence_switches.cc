// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/copresence_switches.h"

// TODO(ckehoe): Move these flags to the chrome://copresence page.

namespace switches {

// Address for calls to the Copresence server (via Apiary).
// Defaults to https://www.googleapis.com/copresence/v2/copresence.
const char kCopresenceServer[] = "copresence-server";

// Apiary tracing token for calls to the Copresence server.
const char kCopresenceTracingToken[] = "copresence-tracing-token";

}  // namespace switches
