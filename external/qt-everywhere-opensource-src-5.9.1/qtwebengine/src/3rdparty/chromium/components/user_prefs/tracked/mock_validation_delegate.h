// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_PREFS_TRACKED_MOCK_VALIDATION_DELEGATE_H_
#define COMPONENTS_USER_PREFS_TRACKED_MOCK_VALIDATION_DELEGATE_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/user_prefs/tracked/pref_hash_filter.h"
#include "components/user_prefs/tracked/pref_hash_store_transaction.h"
#include "components/user_prefs/tracked/tracked_preference_validation_delegate.h"

// A mock tracked preference validation delegate for use by tests.
class MockValidationDelegate : public TrackedPreferenceValidationDelegate {
 public:
  struct ValidationEvent {
    ValidationEvent(
        const std::string& path,
        PrefHashStoreTransaction::ValueState state,
        PrefHashStoreTransaction::ValueState external_validation_state,
        bool is_personal,
        PrefHashFilter::PrefTrackingStrategy tracking_strategy)
        : pref_path(path),
          value_state(state),
          external_validation_value_state(external_validation_state),
          is_personal(is_personal),
          strategy(tracking_strategy) {}

    std::string pref_path;
    PrefHashStoreTransaction::ValueState value_state;
    PrefHashStoreTransaction::ValueState external_validation_value_state;
    bool is_personal;
    PrefHashFilter::PrefTrackingStrategy strategy;
  };

  MockValidationDelegate();
  ~MockValidationDelegate() override;

  // Returns the number of recorded validations.
  size_t recorded_validations_count() const { return validations_.size(); }

  // Returns the number of validations of a given value state.
  size_t CountValidationsOfState(
      PrefHashStoreTransaction::ValueState value_state) const;

  // Returns the number of external validations of a given value state.
  size_t CountExternalValidationsOfState(
      PrefHashStoreTransaction::ValueState value_state) const;

  // Returns the event for the preference with a given path.
  const ValidationEvent* GetEventForPath(const std::string& pref_path) const;

  // TrackedPreferenceValidationDelegate implementation.
  void OnAtomicPreferenceValidation(
      const std::string& pref_path,
      const base::Value* value,
      PrefHashStoreTransaction::ValueState value_state,
      PrefHashStoreTransaction::ValueState external_validation_value_state,
      bool is_personal) override;
  void OnSplitPreferenceValidation(
      const std::string& pref_path,
      const base::DictionaryValue* dict_value,
      const std::vector<std::string>& invalid_keys,
      const std::vector<std::string>& external_validation_invalid_keys,
      PrefHashStoreTransaction::ValueState value_state,
      PrefHashStoreTransaction::ValueState external_validation_value_state,
      bool is_personal) override;

 private:
  // Adds a new validation event.
  void RecordValidation(
      const std::string& pref_path,
      PrefHashStoreTransaction::ValueState value_state,
      PrefHashStoreTransaction::ValueState external_validation_value_state,
      bool is_personal,
      PrefHashFilter::PrefTrackingStrategy strategy);

  std::vector<ValidationEvent> validations_;

  DISALLOW_COPY_AND_ASSIGN(MockValidationDelegate);
};

#endif  // COMPONENTS_USER_PREFS_TRACKED_MOCK_VALIDATION_DELEGATE_H_
