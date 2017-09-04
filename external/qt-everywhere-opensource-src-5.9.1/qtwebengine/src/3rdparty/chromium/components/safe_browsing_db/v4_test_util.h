// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_DB_V4_TEST_UTIL_H_
#define COMPONENTS_SAFE_BROWSING_DB_V4_TEST_UTIL_H_

// Contains methods useful for tests.

#include <ostream>

namespace safe_browsing {

struct ThreatMetadata;
struct V4ProtocolConfig;

V4ProtocolConfig GetTestV4ProtocolConfig(bool disable_auto_update = false);

std::ostream& operator<<(std::ostream& os, const ThreatMetadata& meta);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_DB_V4_TEST_UTIL_H_
