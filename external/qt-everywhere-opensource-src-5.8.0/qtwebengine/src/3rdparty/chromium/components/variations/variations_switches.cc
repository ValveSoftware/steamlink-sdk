// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_switches.h"

namespace variations {
namespace switches {

// Fakes the channel of the browser for purposes of Variations filtering. This
// is to be used for testing only. Possible values are "stable", "beta", "dev"
// and "canary". Note that this only applies if the browser's reported channel
// is UNKNOWN.
const char kFakeVariationsChannel[] = "fake-variations-channel";

// Specifies a custom URL for the server which reports variation data to the
// client. Specifying this switch enables the Variations service on
// unofficial builds. See variations_service.cc.
const char kVariationsServerURL[] = "variations-server-url";

}  // namespace switches
}  // namespace variations
