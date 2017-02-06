// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/processed_study.h"

#include <set>
#include <string>

#include "base/version.h"
#include "components/variations/proto/study.pb.h"

namespace variations {

namespace {

// Validates the sanity of |study| and computes the total probability and
// whether all assignments are to a single group.
bool ValidateStudyAndComputeTotalProbability(
    const Study& study,
    base::FieldTrial::Probability* total_probability,
    bool* all_assignments_to_one_group,
    std::string* single_feature_name) {
  // At the moment, a missing default_experiment_name makes the study invalid.
  if (study.default_experiment_name().empty()) {
    DVLOG(1) << study.name() << " has no default experiment defined.";
    return false;
  }
  if (study.filter().has_min_version() &&
      !Version::IsValidWildcardString(study.filter().min_version())) {
    DVLOG(1) << study.name() << " has invalid min version: "
             << study.filter().min_version();
    return false;
  }
  if (study.filter().has_max_version() &&
      !Version::IsValidWildcardString(study.filter().max_version())) {
    DVLOG(1) << study.name() << " has invalid max version: "
             << study.filter().max_version();
    return false;
  }

  const std::string& default_group_name = study.default_experiment_name();
  base::FieldTrial::Probability divisor = 0;

  bool multiple_assigned_groups = false;
  bool found_default_group = false;
  std::string single_feature_name_seen;
  bool has_multiple_features = false;

  std::set<std::string> experiment_names;
  for (int i = 0; i < study.experiment_size(); ++i) {
    const Study_Experiment& experiment = study.experiment(i);
    if (experiment.name().empty()) {
      DVLOG(1) << study.name() << " is missing experiment " << i << " name";
      return false;
    }
    if (!experiment_names.insert(experiment.name()).second) {
      DVLOG(1) << study.name() << " has a repeated experiment name "
               << study.experiment(i).name();
      return false;
    }

    if (!has_multiple_features) {
      const auto& features = experiment.feature_association();
      for (int i = 0; i < features.enable_feature_size(); ++i) {
        const std::string& feature_name = features.enable_feature(i);
        if (single_feature_name_seen.empty()) {
          single_feature_name_seen = feature_name;
        } else if (feature_name != single_feature_name_seen) {
          has_multiple_features = true;
          break;
        }
      }
      for (int i = 0; i < features.disable_feature_size(); ++i) {
        const std::string& feature_name = features.disable_feature(i);
        if (single_feature_name_seen.empty()) {
          single_feature_name_seen = feature_name;
        } else if (feature_name != single_feature_name_seen) {
          has_multiple_features = true;
          break;
        }
      }
    }

    if (!experiment.has_forcing_flag() && experiment.probability_weight() > 0) {
      // If |divisor| is not 0, there was at least one prior non-zero group.
      if (divisor != 0)
        multiple_assigned_groups = true;
      divisor += experiment.probability_weight();
    }
    if (study.experiment(i).name() == default_group_name)
      found_default_group = true;
  }

  if (!found_default_group) {
    DVLOG(1) << study.name() << " is missing default experiment in its "
             << "experiment list";
    // The default group was not found in the list of groups. This study is not
    // valid.
    return false;
  }

  if (!has_multiple_features && !single_feature_name_seen.empty())
    single_feature_name->swap(single_feature_name_seen);
  else
    single_feature_name->clear();

  *total_probability = divisor;
  *all_assignments_to_one_group = !multiple_assigned_groups;
  return true;
}


}  // namespace

ProcessedStudy::ProcessedStudy()
    : study_(NULL),
      total_probability_(0),
      all_assignments_to_one_group_(false),
      is_expired_(false) {
}

ProcessedStudy::~ProcessedStudy() {
}

bool ProcessedStudy::Init(const Study* study, bool is_expired) {
  base::FieldTrial::Probability total_probability = 0;
  bool all_assignments_to_one_group = false;
  std::string single_feature_name;
  if (!ValidateStudyAndComputeTotalProbability(*study, &total_probability,
                                               &all_assignments_to_one_group,
                                               &single_feature_name)) {
    return false;
  }

  study_ = study;
  is_expired_ = is_expired;
  total_probability_ = total_probability;
  all_assignments_to_one_group_ = all_assignments_to_one_group;
  single_feature_name_.swap(single_feature_name);
  return true;
}

int ProcessedStudy::GetExperimentIndexByName(const std::string& name) const {
  for (int i = 0; i < study_->experiment_size(); ++i) {
    if (study_->experiment(i).name() == name)
      return i;
  }

  return -1;
}

// static
bool ProcessedStudy::ValidateAndAppendStudy(
    const Study* study,
    bool is_expired,
    std::vector<ProcessedStudy>* processed_studies) {
  ProcessedStudy processed_study;
  if (processed_study.Init(study, is_expired)) {
    processed_studies->push_back(processed_study);
    return true;
  }
  return false;
}

}  // namespace variations
