// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_prefs/tracked/tracked_preference_helper.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "components/user_prefs/tracked/tracked_preference_histogram_names.h"

TrackedPreferenceHelper::TrackedPreferenceHelper(
    const std::string& pref_path,
    size_t reporting_id,
    size_t reporting_ids_count,
    PrefHashFilter::EnforcementLevel enforcement_level,
    PrefHashFilter::ValueType value_type)
    : pref_path_(pref_path),
      reporting_id_(reporting_id),
      reporting_ids_count_(reporting_ids_count),
      enforce_(enforcement_level == PrefHashFilter::ENFORCE_ON_LOAD),
      personal_(value_type == PrefHashFilter::VALUE_PERSONAL) {
}

TrackedPreferenceHelper::ResetAction TrackedPreferenceHelper::GetAction(
    PrefHashStoreTransaction::ValueState value_state) const {
  switch (value_state) {
    case PrefHashStoreTransaction::UNCHANGED:
      // Desired case, nothing to do.
      return DONT_RESET;
    case PrefHashStoreTransaction::CLEARED:
      // Unfortunate case, but there is nothing we can do.
      return DONT_RESET;
    case PrefHashStoreTransaction::TRUSTED_NULL_VALUE:  // Falls through.
    case PrefHashStoreTransaction::TRUSTED_UNKNOWN_VALUE:
      // It is okay to seed the hash in this case.
      return DONT_RESET;
    case PrefHashStoreTransaction::SECURE_LEGACY:
      // Accept secure legacy device ID based hashes.
      return DONT_RESET;
    case PrefHashStoreTransaction::UNTRUSTED_UNKNOWN_VALUE:  // Falls through.
    case PrefHashStoreTransaction::CHANGED:
      return enforce_ ? DO_RESET : WANTED_RESET;
  }
  NOTREACHED() << "Unexpected PrefHashStoreTransaction::ValueState: "
               << value_state;
  return DONT_RESET;
}

bool TrackedPreferenceHelper::IsPersonal() const {
  return personal_;
}

void TrackedPreferenceHelper::ReportValidationResult(
    PrefHashStoreTransaction::ValueState value_state) const {
  switch (value_state) {
    case PrefHashStoreTransaction::UNCHANGED:
      UMA_HISTOGRAM_ENUMERATION(
          user_prefs::tracked::kTrackedPrefHistogramUnchanged, reporting_id_,
          reporting_ids_count_);
      return;
    case PrefHashStoreTransaction::CLEARED:
      UMA_HISTOGRAM_ENUMERATION(
          user_prefs::tracked::kTrackedPrefHistogramCleared, reporting_id_,
          reporting_ids_count_);
      return;
    case PrefHashStoreTransaction::SECURE_LEGACY:
      UMA_HISTOGRAM_ENUMERATION(
          user_prefs::tracked::kTrackedPrefHistogramMigratedLegacyDeviceId,
          reporting_id_, reporting_ids_count_);
      return;
    case PrefHashStoreTransaction::CHANGED:
      UMA_HISTOGRAM_ENUMERATION(
          user_prefs::tracked::kTrackedPrefHistogramChanged, reporting_id_,
          reporting_ids_count_);
      return;
    case PrefHashStoreTransaction::UNTRUSTED_UNKNOWN_VALUE:
      UMA_HISTOGRAM_ENUMERATION(
          user_prefs::tracked::kTrackedPrefHistogramInitialized, reporting_id_,
          reporting_ids_count_);
      return;
    case PrefHashStoreTransaction::TRUSTED_UNKNOWN_VALUE:
      UMA_HISTOGRAM_ENUMERATION(
          user_prefs::tracked::kTrackedPrefHistogramTrustedInitialized,
          reporting_id_, reporting_ids_count_);
      return;
    case PrefHashStoreTransaction::TRUSTED_NULL_VALUE:
      UMA_HISTOGRAM_ENUMERATION(
          user_prefs::tracked::kTrackedPrefHistogramNullInitialized,
          reporting_id_, reporting_ids_count_);
      return;
  }
  NOTREACHED() << "Unexpected PrefHashStoreTransaction::ValueState: "
               << value_state;
}

void TrackedPreferenceHelper::ReportAction(ResetAction reset_action) const {
  switch (reset_action) {
    case DONT_RESET:
      // No report for DONT_RESET.
      break;
    case WANTED_RESET:
      UMA_HISTOGRAM_ENUMERATION(
          user_prefs::tracked::kTrackedPrefHistogramWantedReset, reporting_id_,
          reporting_ids_count_);
      break;
    case DO_RESET:
      UMA_HISTOGRAM_ENUMERATION(user_prefs::tracked::kTrackedPrefHistogramReset,
                                reporting_id_, reporting_ids_count_);
      break;
  }
}

void TrackedPreferenceHelper::ReportSplitPreferenceChangedCount(
    size_t count) const {
  // The histogram below is an expansion of the UMA_HISTOGRAM_COUNTS_100 macro
  // adapted to allow for a dynamically suffixed histogram name.
  // Note: The factory creates and owns the histogram.
  base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
      user_prefs::tracked::kTrackedSplitPrefHistogramChanged + pref_path_, 1,
      100,  // Allow counts up to 100.
      101, base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(count);
}
