// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing_db/util.h"
#include "components/safe_browsing_db/v4_test_util.h"

namespace safe_browsing {

namespace {

const char kClient[] = "unittest";
const char kAppVer[] = "1.0";
const char kKeyParam[] = "test_key_param";

}  // namespace

V4ProtocolConfig GetTestV4ProtocolConfig(bool disable_auto_update) {
  return V4ProtocolConfig(kClient, disable_auto_update, kKeyParam, kAppVer);
}

std::ostream& operator<<(std::ostream& os, const ThreatMetadata& meta) {
  os << "{threat_pattern_type=" << static_cast<int>(meta.threat_pattern_type)
     << ", api_permissions=[";
  for (auto p : meta.api_permissions)
    os << p << ",";
  return os << "], population_id=" << meta.population_id << "}";
}

}  // namespace safe_browsing
