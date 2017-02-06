// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/study_filtering.h"

#include <stddef.h>
#include <stdint.h>

#include <set>

#include "base/stl_util.h"
#include "build/build_config.h"

namespace variations {

namespace {

Study_Platform GetCurrentPlatform() {
#if defined(OS_WIN)
  return Study_Platform_PLATFORM_WINDOWS;
#elif defined(OS_IOS)
  return Study_Platform_PLATFORM_IOS;
#elif defined(OS_MACOSX)
  return Study_Platform_PLATFORM_MAC;
#elif defined(OS_CHROMEOS)
  return Study_Platform_PLATFORM_CHROMEOS;
#elif defined(OS_ANDROID)
  return Study_Platform_PLATFORM_ANDROID;
#elif defined(OS_LINUX) || defined(OS_BSD) || defined(OS_SOLARIS)
  // Default BSD and SOLARIS to Linux to not break those builds, although these
  // platforms are not officially supported by Chrome.
  return Study_Platform_PLATFORM_LINUX;
#else
#error Unknown platform
#endif
}

// Converts |date_time| in Study date format to base::Time.
base::Time ConvertStudyDateToBaseTime(int64_t date_time) {
  return base::Time::UnixEpoch() + base::TimeDelta::FromSeconds(date_time);
}

}  // namespace

namespace internal {

bool CheckStudyChannel(const Study_Filter& filter, Study_Channel channel) {
  // An empty channel list matches all channels.
  if (filter.channel_size() == 0)
    return true;

  for (int i = 0; i < filter.channel_size(); ++i) {
    if (filter.channel(i) == channel)
      return true;
  }
  return false;
}

bool CheckStudyFormFactor(const Study_Filter& filter,
                          Study_FormFactor form_factor) {
  // An empty form factor list matches all form factors.
  if (filter.form_factor_size() == 0)
    return true;

  for (int i = 0; i < filter.form_factor_size(); ++i) {
    if (filter.form_factor(i) == form_factor)
      return true;
  }
  return false;
}

bool CheckStudyHardwareClass(const Study_Filter& filter,
                             const std::string& hardware_class) {
  // Empty hardware_class and exclude_hardware_class matches all.
  if (filter.hardware_class_size() == 0 &&
      filter.exclude_hardware_class_size() == 0) {
    return true;
  }

  // Checks if we are supposed to filter for a specified set of
  // hardware_classes. Note that this means this overrides the
  // exclude_hardware_class in case that ever occurs (which it shouldn't).
  if (filter.hardware_class_size() > 0) {
    for (int i = 0; i < filter.hardware_class_size(); ++i) {
      // Check if the entry is a substring of |hardware_class|.
      size_t position = hardware_class.find(filter.hardware_class(i));
      if (position != std::string::npos)
        return true;
    }
    // None of the requested hardware_classes match.
    return false;
  }

  // Omit if matches any of the exclude entries.
  for (int i = 0; i < filter.exclude_hardware_class_size(); ++i) {
    // Check if the entry is a substring of |hardware_class|.
    size_t position = hardware_class.find(
        filter.exclude_hardware_class(i));
    if (position != std::string::npos)
      return false;
  }

  // None of the exclusions match, so this accepts.
  return true;
}

bool CheckStudyLocale(const Study_Filter& filter, const std::string& locale) {
  // An empty locale list matches all locales.
  if (filter.locale_size() == 0)
    return true;

  for (int i = 0; i < filter.locale_size(); ++i) {
    if (filter.locale(i) == locale)
      return true;
  }
  return false;
}

bool CheckStudyPlatform(const Study_Filter& filter, Study_Platform platform) {
  // An empty platform list matches all platforms.
  if (filter.platform_size() == 0)
    return true;

  for (int i = 0; i < filter.platform_size(); ++i) {
    if (filter.platform(i) == platform)
      return true;
  }
  return false;
}

bool CheckStudyStartDate(const Study_Filter& filter,
                         const base::Time& date_time) {
  if (filter.has_start_date()) {
    const base::Time start_date =
        ConvertStudyDateToBaseTime(filter.start_date());
    return date_time >= start_date;
  }

  return true;
}

bool CheckStudyVersion(const Study_Filter& filter,
                       const base::Version& version) {
  if (filter.has_min_version()) {
    if (version.CompareToWildcardString(filter.min_version()) < 0)
      return false;
  }

  if (filter.has_max_version()) {
    if (version.CompareToWildcardString(filter.max_version()) > 0)
      return false;
  }

  return true;
}

bool CheckStudyCountry(const Study_Filter& filter, const std::string& country) {
  // Empty country and exclude_country matches all.
  if (filter.country_size() == 0 && filter.exclude_country_size() == 0)
    return true;

  // Checks if we are supposed to filter for a specified set of countries. Note
  // that this means this overrides the exclude_country in case that ever occurs
  // (which it shouldn't).
  if (filter.country_size() > 0)
    return ContainsValue(filter.country(), country);

  // Omit if matches any of the exclude entries.
  return !ContainsValue(filter.exclude_country(), country);
}

bool IsStudyExpired(const Study& study, const base::Time& date_time) {
  if (study.has_expiry_date()) {
    const base::Time expiry_date =
        ConvertStudyDateToBaseTime(study.expiry_date());
    return date_time >= expiry_date;
  }

  return false;
}

bool ShouldAddStudy(
    const Study& study,
    const std::string& locale,
    const base::Time& reference_date,
    const base::Version& version,
    Study_Channel channel,
    Study_FormFactor form_factor,
    const std::string& hardware_class,
    const std::string& country) {
  if (study.has_filter()) {
    if (!CheckStudyChannel(study.filter(), channel)) {
      DVLOG(1) << "Filtered out study " << study.name() << " due to channel.";
      return false;
    }

    if (!CheckStudyFormFactor(study.filter(), form_factor)) {
      DVLOG(1) << "Filtered out study " << study.name() <<
                  " due to form factor.";
      return false;
    }

    if (!CheckStudyLocale(study.filter(), locale)) {
      DVLOG(1) << "Filtered out study " << study.name() << " due to locale.";
      return false;
    }

    if (!CheckStudyPlatform(study.filter(), GetCurrentPlatform())) {
      DVLOG(1) << "Filtered out study " << study.name() << " due to platform.";
      return false;
    }

    if (!CheckStudyVersion(study.filter(), version)) {
      DVLOG(1) << "Filtered out study " << study.name() << " due to version.";
      return false;
    }

    if (!CheckStudyStartDate(study.filter(), reference_date)) {
      DVLOG(1) << "Filtered out study " << study.name() <<
                  " due to start date.";
      return false;
    }

    if (!CheckStudyHardwareClass(study.filter(), hardware_class)) {
      DVLOG(1) << "Filtered out study " << study.name() <<
                  " due to hardware_class.";
      return false;
    }

    if (!CheckStudyCountry(study.filter(), country)) {
      DVLOG(1) << "Filtered out study " << study.name() << " due to country.";
      return false;
    }
  }

  DVLOG(1) << "Kept study " << study.name() << ".";
  return true;
}

}  // namespace internal

void FilterAndValidateStudies(const VariationsSeed& seed,
                              const std::string& locale,
                              const base::Time& reference_date,
                              const base::Version& version,
                              Study_Channel channel,
                              Study_FormFactor form_factor,
                              const std::string& hardware_class,
                              const std::string& session_consistency_country,
                              const std::string& permanent_consistency_country,
                              std::vector<ProcessedStudy>* filtered_studies) {
  DCHECK(version.IsValid());

  // Add expired studies (in a disabled state) only after all the non-expired
  // studies have been added (and do not add an expired study if a corresponding
  // non-expired study got added). This way, if there's both an expired and a
  // non-expired study that applies, the non-expired study takes priority.
  std::set<std::string> created_studies;
  std::vector<const Study*> expired_studies;

  for (int i = 0; i < seed.study_size(); ++i) {
    const Study& study = seed.study(i);

    // Unless otherwise specified, use an empty country that won't pass any
    // filters that specifically include countries, but will pass any filters
    // that specifically exclude countries.
    std::string country;
    switch (study.consistency()) {
      case Study_Consistency_SESSION:
        country = session_consistency_country;
        break;
      case Study_Consistency_PERMANENT:
        // Use the saved |permanent_consistency_country| for permanent
        // consistency studies. This allows Chrome to use the same country for
        // filtering permanent consistency studies between Chrome upgrades.
        // Since some studies have user-visible effects, this helps to avoid
        // annoying users with experimental group churn while traveling.
        country = permanent_consistency_country;
        break;
    }

    if (!internal::ShouldAddStudy(study, locale, reference_date, version,
                                  channel, form_factor, hardware_class,
                                  country)) {
      continue;
    }

    if (internal::IsStudyExpired(study, reference_date)) {
      expired_studies.push_back(&study);
    } else if (!ContainsKey(created_studies, study.name())) {
      ProcessedStudy::ValidateAndAppendStudy(&study, false, filtered_studies);
      created_studies.insert(study.name());
    }
  }

  for (size_t i = 0; i < expired_studies.size(); ++i) {
    if (!ContainsKey(created_studies, expired_studies[i]->name())) {
      ProcessedStudy::ValidateAndAppendStudy(expired_studies[i], true,
                                             filtered_studies);
    }
  }
}

}  // namespace variations
