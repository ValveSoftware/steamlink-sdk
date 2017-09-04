// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_STUDY_FILTERING_H_
#define COMPONENTS_VARIATIONS_STUDY_FILTERING_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/variations/processed_study.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/proto/variations_seed.pb.h"

namespace variations {

// Internal functions exposed for testing purposes only.
namespace internal {

// Checks whether a study is applicable for the given |channel| per |filter|.
bool CheckStudyChannel(const Study_Filter& filter, Study_Channel channel);

// Checks whether a study is applicable for the given |form_factor| per
// |filter|.
bool CheckStudyFormFactor(const Study_Filter& filter,
                          Study_FormFactor form_factor);

// Checks whether a study is applicable for the given |hardware_class| per
// |filter|.
bool CheckStudyHardwareClass(const Study_Filter& filter,
                             const std::string& hardware_class);

// Checks whether a study is applicable for the given |locale| per |filter|.
bool CheckStudyLocale(const Study_Filter& filter, const std::string& locale);

// Checks whether a study is applicable for the given |platform| per |filter|.
bool CheckStudyPlatform(const Study_Filter& filter, Study_Platform platform);

// Checks whether a study is applicable for the given date/time per |filter|.
bool CheckStudyStartDate(const Study_Filter& filter,
                         const base::Time& date_time);

// Checks whether a study is applicable for the given version per |filter|.
bool CheckStudyVersion(const Study_Filter& filter,
                       const base::Version& version);

// Checks whether a study is applicable for the given |country| per |filter|.
bool CheckStudyCountry(const Study_Filter& filter, const std::string& country);

// Checks whether |study| is expired using the given date/time.
bool IsStudyExpired(const Study& study, const base::Time& date_time);

// Returns whether |study| should be disabled according to its restriction
// parameters.
bool ShouldAddStudy(const Study& study,
                    const std::string& locale,
                    const base::Time& reference_date,
                    const base::Version& version,
                    Study_Channel channel,
                    Study_FormFactor form_factor,
                    const std::string& hardware_class,
                    const std::string& country);

}  // namespace internal

// Filters the list of studies in |seed| and validates and pre-processes them,
// adding any kept studies to |filtered_studies| list. Ensures that the
// resulting list will not have more than one study with the same name.
void FilterAndValidateStudies(const VariationsSeed& seed,
                              const std::string& locale,
                              const base::Time& reference_date,
                              const base::Version& version,
                              Study_Channel channel,
                              Study_FormFactor form_factor,
                              const std::string& hardware_class,
                              const std::string& session_consistency_country,
                              const std::string& permanent_consistency_country,
                              std::vector<ProcessedStudy>* filtered_studies);

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_STUDY_FILTERING_H_
