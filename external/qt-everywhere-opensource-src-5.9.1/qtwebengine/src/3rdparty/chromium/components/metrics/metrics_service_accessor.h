// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_SERVICE_ACCESSOR_H_
#define COMPONENTS_METRICS_METRICS_SERVICE_ACCESSOR_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/macros.h"

class PrefService;

namespace metrics {

class MetricsService;

// This class limits and documents access to metrics service helper methods.
// These methods are protected so each user has to inherit own program-specific
// specialization and enable access there by declaring friends.
class MetricsServiceAccessor {
 protected:
  // Constructor declared as protected to enable inheritance. Descendants should
  // disallow instantiation.
  MetricsServiceAccessor() {}

  // Returns whether metrics reporting is enabled, using the value of the
  // kMetricsReportingEnabled pref in |pref_service| to determine whether user
  // has enabled reporting.
  static bool IsMetricsReportingEnabled(PrefService* pref_service);


  // Registers a field trial name and group with |metrics_service| (if not
  // null), to be used to annotate a UMA report with a particular configuration
  // state. Returns true on success.
  // See the comment on MetricsService::RegisterSyntheticFieldTrial() for
  // details.
  static bool RegisterSyntheticFieldTrial(MetricsService* metrics_service,
                                          const std::string& trial_name,
                                          const std::string& group_name);

  // Registers a field trial name and set of groups with |metrics_service| (if
  // not null), to be used to annotate a UMA report with a particular
  // configuration state. Returns true on success.
  // See the comment on MetricsService::RegisterSyntheticMultiGroupFieldTrial()
  // for details.
  static bool RegisterSyntheticMultiGroupFieldTrial(
      MetricsService* metrics_service,
      const std::string& trial_name,
      const std::vector<uint32_t>& group_name_hashes);

  // Same as RegisterSyntheticFieldTrial above, but takes in the trial name as a
  // hash rather than computing the hash from the string.
  static bool RegisterSyntheticFieldTrialWithNameHash(
      MetricsService* metrics_service,
      uint32_t trial_name_hash,
      const std::string& group_name);

  // Same as RegisterSyntheticFieldTrial above, but takes in the trial and group
  // names as hashes rather than computing those hashes from the strings.
  static bool RegisterSyntheticFieldTrialWithNameAndGroupHash(
      MetricsService* metrics_service,
      uint32_t trial_name_hash,
      uint32_t group_name_hash);

 private:
  DISALLOW_COPY_AND_ASSIGN(MetricsServiceAccessor);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_SERVICE_ACCESSOR_H_
