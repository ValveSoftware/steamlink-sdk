// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing_db/safe_browsing_api_handler_util.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/json/json_reader.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/safe_browsing_db/metadata.pb.h"
#include "components/safe_browsing_db/util.h"

namespace safe_browsing {
namespace {

// JSON metatdata keys. These are are fixed in the Java-side API.
const char kJsonKeyMatches[] = "matches";
const char kJsonKeyThreatType[] = "threat_type";

// Do not reorder or delete.  Make sure changes are reflected in
// SB2RemoteCallThreatSubType.
enum UmaThreatSubType {
  UMA_THREAT_SUB_TYPE_NOT_SET = 0,
  UMA_THREAT_SUB_TYPE_POTENTIALLY_HALMFUL_APP_LANDING = 1,
  UMA_THREAT_SUB_TYPE_POTENTIALLY_HALMFUL_APP_DISTRIBUTION = 2,
  UMA_THREAT_SUB_TYPE_UNKNOWN = 3,
  UMA_THREAT_SUB_TYPE_SOCIAL_ENGINEERING_ADS = 4,
  UMA_THREAT_SUB_TYPE_SOCIAL_ENGINEERING_LANDING = 5,
  UMA_THREAT_SUB_TYPE_PHISHING = 6,
  UMA_THREAT_SUB_TYPE_MAX_VALUE
};

void ReportUmaThreatSubType(SBThreatType threat_type,
                            UmaThreatSubType sub_type) {
  if (threat_type == SB_THREAT_TYPE_URL_MALWARE) {
    UMA_HISTOGRAM_ENUMERATION(
        "SB2.RemoteCall.ThreatSubType.PotentiallyHarmfulApp", sub_type,
        UMA_THREAT_SUB_TYPE_MAX_VALUE);
  } else {
    UMA_HISTOGRAM_ENUMERATION("SB2.RemoteCall.ThreatSubType.SocialEngineering",
                              sub_type, UMA_THREAT_SUB_TYPE_MAX_VALUE);
  }
}

// Parse the appropriate "*_pattern_type" key from the metadata.
// Returns NONE if no pattern type was found.
ThreatPatternType ParseThreatSubType(
    const base::DictionaryValue* match,
    SBThreatType threat_type) {
  std::string pattern_key;
  if (threat_type == SB_THREAT_TYPE_URL_MALWARE) {
    pattern_key = "pha_pattern_type";
  } else {
    DCHECK(threat_type == SB_THREAT_TYPE_URL_PHISHING);
    pattern_key = "se_pattern_type";
  }

  std::string pattern_type;
  if (!match->GetString(pattern_key, &pattern_type)) {
    ReportUmaThreatSubType(threat_type, UMA_THREAT_SUB_TYPE_NOT_SET);
    return ThreatPatternType::NONE;
  }

  if (threat_type == SB_THREAT_TYPE_URL_MALWARE) {
    if (pattern_type == "LANDING") {
      ReportUmaThreatSubType(
          threat_type, UMA_THREAT_SUB_TYPE_POTENTIALLY_HALMFUL_APP_LANDING);
      return ThreatPatternType::MALWARE_LANDING;
    } else if (pattern_type == "DISTRIBUTION") {
      ReportUmaThreatSubType(
          threat_type,
          UMA_THREAT_SUB_TYPE_POTENTIALLY_HALMFUL_APP_DISTRIBUTION);
      return ThreatPatternType::MALWARE_DISTRIBUTION;
    } else {
      ReportUmaThreatSubType(threat_type, UMA_THREAT_SUB_TYPE_UNKNOWN);
      return ThreatPatternType::NONE;
    }
  } else {
    DCHECK(threat_type == SB_THREAT_TYPE_URL_PHISHING);
    if (pattern_type == "SOCIAL_ENGINEERING_ADS") {
      ReportUmaThreatSubType(threat_type,
                             UMA_THREAT_SUB_TYPE_SOCIAL_ENGINEERING_ADS);
      return ThreatPatternType::SOCIAL_ENGINEERING_ADS;
    } else if (pattern_type == "SOCIAL_ENGINEERING_LANDING") {
      ReportUmaThreatSubType(threat_type,
                             UMA_THREAT_SUB_TYPE_SOCIAL_ENGINEERING_LANDING);
      return ThreatPatternType::SOCIAL_ENGINEERING_LANDING;
    } else if (pattern_type == "PHISHING") {
      ReportUmaThreatSubType(threat_type, UMA_THREAT_SUB_TYPE_PHISHING);
      return ThreatPatternType::PHISHING;
    } else {
      ReportUmaThreatSubType(threat_type, UMA_THREAT_SUB_TYPE_UNKNOWN);
      return ThreatPatternType::NONE;
    }
  }
}

// Parse the optional "UserPopulation" key from the metadata.
// Returns empty string if none was found.
std::string ParseUserPopulation(const base::DictionaryValue* match) {
  std::string population_id;
  if (!match->GetString("UserPopulation", &population_id))
    return std::string();
  else
    return population_id;
}

int GetThreatSeverity(int java_threat_num) {
  // Assign higher numbers to more severe threats.
  switch (java_threat_num) {
    case JAVA_THREAT_TYPE_POTENTIALLY_HARMFUL_APPLICATION:
      return 2;
    case JAVA_THREAT_TYPE_SOCIAL_ENGINEERING:
      return 1;
    default:
      // Unknown threat type
      return -1;
  }
}

SBThreatType JavaToSBThreatType(int java_threat_num) {
  switch (java_threat_num) {
    case JAVA_THREAT_TYPE_POTENTIALLY_HARMFUL_APPLICATION:
      return SB_THREAT_TYPE_URL_MALWARE;
    case JAVA_THREAT_TYPE_SOCIAL_ENGINEERING:
      return SB_THREAT_TYPE_URL_PHISHING;
    default:
      // Unknown threat type
      return SB_THREAT_TYPE_SAFE;
  }
}

}  // namespace

// Valid examples:
// {"matches":[{"threat_type":"5"}]}
//   or
// {"matches":[{"threat_type":"4", "pha_pattern_type":"LANDING"},
//             {"threat_type":"5"}]}
//   or
// {"matches":[{"threat_type":"4", "UserPopulation":"YXNvZWZpbmFqO..."}]
UmaRemoteCallResult ParseJsonFromGMSCore(const std::string& metadata_str,
                                         SBThreatType* worst_threat,
                                         ThreatMetadata* metadata) {
  *worst_threat = SB_THREAT_TYPE_SAFE;  // Default to safe.
  *metadata = ThreatMetadata();  // Default values.

  if (metadata_str.empty())
    return UMA_STATUS_JSON_EMPTY;

  // Pick out the "matches" list.
  std::unique_ptr<base::Value> value = base::JSONReader::Read(metadata_str);
  const base::ListValue* matches = nullptr;
  if (!value.get() || !value->IsType(base::Value::TYPE_DICTIONARY) ||
      !(static_cast<base::DictionaryValue*>(value.get()))
           ->GetList(kJsonKeyMatches, &matches) ||
      !matches) {
    return UMA_STATUS_JSON_FAILED_TO_PARSE;
  }

  // Go through each matched threat type and pick the most severe.
  int worst_threat_num = -1;
  const base::DictionaryValue* worst_match = nullptr;
  for (size_t i = 0; i < matches->GetSize(); i++) {
    // Get the threat number
    const base::DictionaryValue* match;
    std::string threat_num_str;
    int java_threat_num = -1;
    if (!matches->GetDictionary(i, &match) ||
        !match->GetString(kJsonKeyThreatType, &threat_num_str) ||
        !base::StringToInt(threat_num_str, &java_threat_num)) {
      continue;  // Skip malformed list entries
    }

    if (GetThreatSeverity(java_threat_num) >
        GetThreatSeverity(worst_threat_num)) {
      worst_threat_num = java_threat_num;
      worst_match = match;
    }
  }

  *worst_threat = JavaToSBThreatType(worst_threat_num);
  if (*worst_threat == SB_THREAT_TYPE_SAFE || !worst_match)
    return UMA_STATUS_JSON_UNKNOWN_THREAT;

  // Fill in the metadata
  metadata->threat_pattern_type =
      ParseThreatSubType(worst_match, *worst_threat);
  metadata->population_id = ParseUserPopulation(worst_match);

  return UMA_STATUS_UNSAFE;  // success
}

}  // namespace safe_browsing
