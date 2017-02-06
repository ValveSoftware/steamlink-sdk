// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/switches.h"

namespace ntp_snippets {
namespace switches {

const char kFetchingIntervalWifiChargingSeconds[] =
    "ntp-snippets-fetching-interval-wifi-charging";
const char kFetchingIntervalWifiSeconds[] =
    "ntp-snippets-fetching-interval-wifi";
const char kFetchingIntervalFallbackSeconds[] =
    "ntp-snippets-fetching-interval-fallback";

// If this flag is set, the snippets won't be restricted to the user's NTP
// suggestions.
const char kDontRestrict[] = "ntp-snippets-dont-restrict";

// If this flag is set, we will add downloaded snippets that are missing some
// critical data to the list.
const char kAddIncompleteSnippets[] = "ntp-snippets-add-incomplete";

}  // namespace switches
}  // namespace ntp_snippets
