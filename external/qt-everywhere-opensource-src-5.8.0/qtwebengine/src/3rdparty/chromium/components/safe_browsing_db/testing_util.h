// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utilities to be used in tests of safe_browsing_db/ component.

#ifndef COMPONENTS_SAFE_BROWSING_DB_TESTING_UTIL_H_
#define COMPONENTS_SAFE_BROWSING_DB_TESTING_UTIL_H_

#include "components/safe_browsing_db/util.h"

#include <ostream>

namespace safe_browsing {

inline bool operator==(const ThreatMetadata& lhs, const ThreatMetadata& rhs) {
  return lhs.threat_pattern_type == rhs.threat_pattern_type &&
         lhs.api_permissions == rhs.api_permissions &&
         lhs.population_id == rhs.population_id;
}

inline bool operator!=(const ThreatMetadata& lhs, const ThreatMetadata& rhs) {
  return !(lhs == rhs);
}

inline std::ostream& operator<<(std::ostream& os, const ThreatMetadata& meta) {
  os << "{threat_pattern_type=" << static_cast<int>(meta.threat_pattern_type)
     << ", api_permissions=[";
  for (auto p : meta.api_permissions)
    os << p << ",";
  return os << "], population_id=" << meta.population_id;
}

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_DB_TESTING_UTIL_H_
