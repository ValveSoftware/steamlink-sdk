// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_PREFS_TRACKED_TRACKED_SPLIT_PREFERENCE_H_
#define COMPONENTS_USER_PREFS_TRACKED_TRACKED_SPLIT_PREFERENCE_H_

#include <stddef.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/user_prefs/tracked/pref_hash_filter.h"
#include "components/user_prefs/tracked/tracked_preference.h"
#include "components/user_prefs/tracked/tracked_preference_helper.h"

class TrackedPreferenceValidationDelegate;

// A TrackedSplitPreference must be tracking a dictionary pref. Each top-level
// entry in its dictionary is tracked and enforced independently. An optional
// delegate is notified of the status of the preference during enforcement.
// Note: preferences using this strategy must be kept in sync with
// TrackedSplitPreferences in histograms.xml.
class TrackedSplitPreference : public TrackedPreference {
 public:
  // Constructs a TrackedSplitPreference. |pref_path| must be a dictionary pref.
  TrackedSplitPreference(const std::string& pref_path,
                         size_t reporting_id,
                         size_t reporting_ids_count,
                         PrefHashFilter::EnforcementLevel enforcement_level,
                         PrefHashFilter::ValueType value_type,
                         TrackedPreferenceValidationDelegate* delegate);

  // TrackedPreference implementation.
  TrackedPreferenceType GetType() const override;
  void OnNewValue(const base::Value* value,
                  PrefHashStoreTransaction* transaction) const override;
  bool EnforceAndReport(
      base::DictionaryValue* pref_store_contents,
      PrefHashStoreTransaction* transaction,
      PrefHashStoreTransaction* external_validation_transaction) const override;

 private:
  const std::string pref_path_;
  const TrackedPreferenceHelper helper_;
  TrackedPreferenceValidationDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(TrackedSplitPreference);
};

#endif  // COMPONENTS_USER_PREFS_TRACKED_TRACKED_SPLIT_PREFERENCE_H_
