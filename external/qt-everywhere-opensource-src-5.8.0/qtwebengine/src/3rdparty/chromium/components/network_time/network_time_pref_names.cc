// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_time/network_time_pref_names.h"

namespace network_time {
namespace prefs {

// Stores a pair of local time and corresponding network time to bootstrap
// network time tracker when browser starts.
const char kNetworkTimeMapping[] = "network_time.network_time_mapping";

}  // namespace prefs
}  // namespace network_time
