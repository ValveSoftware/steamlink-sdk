// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_PREFS_TRACKED_TRACKED_PREFERENCE_VALIDATION_DELEGATE_H_
#define COMPONENTS_USER_PREFS_TRACKED_TRACKED_PREFERENCE_VALIDATION_DELEGATE_H_

#include <string>
#include <vector>

#include "components/user_prefs/tracked/pref_hash_store_transaction.h"

namespace base {
class DictionaryValue;
class Value;
}

// A TrackedPreferenceValidationDelegate is notified of the results of each
// tracked preference validation event.
class TrackedPreferenceValidationDelegate {
 public:
  virtual ~TrackedPreferenceValidationDelegate() {}

  // Notifies observes of the result (|value_state|) of checking the atomic
  // |value| (which may be NULL) at |pref_path|. |is_personal| indicates whether
  // or not the value may contain personal information.
  virtual void OnAtomicPreferenceValidation(
      const std::string& pref_path,
      const base::Value* value,
      PrefHashStoreTransaction::ValueState value_state,
      PrefHashStoreTransaction::ValueState external_validation_value_state,
      bool is_personal) = 0;

  // Notifies observes of the result (|value_state|) of checking the split
  // |dict_value| (which may be NULL) at |pref_path|. |is_personal| indicates
  // whether or not the value may contain personal information.
  virtual void OnSplitPreferenceValidation(
      const std::string& pref_path,
      const base::DictionaryValue* dict_value,
      const std::vector<std::string>& invalid_keys,
      const std::vector<std::string>& external_validation_invalid_keys,
      PrefHashStoreTransaction::ValueState value_state,
      PrefHashStoreTransaction::ValueState external_validation_value_state,
      bool is_personal) = 0;
};

#endif  // COMPONENTS_USER_PREFS_TRACKED_TRACKED_PREFERENCE_VALIDATION_DELEGATE_H_
