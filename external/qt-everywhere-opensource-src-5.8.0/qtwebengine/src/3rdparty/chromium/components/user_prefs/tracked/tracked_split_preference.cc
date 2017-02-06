// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_prefs/tracked/tracked_split_preference.h"

#include <vector>

#include "base/logging.h"
#include "base/values.h"
#include "components/user_prefs/tracked/pref_hash_store_transaction.h"
#include "components/user_prefs/tracked/tracked_preference_validation_delegate.h"

TrackedSplitPreference::TrackedSplitPreference(
    const std::string& pref_path,
    size_t reporting_id,
    size_t reporting_ids_count,
    PrefHashFilter::EnforcementLevel enforcement_level,
    PrefHashFilter::ValueType value_type,
    TrackedPreferenceValidationDelegate* delegate)
    : pref_path_(pref_path),
      helper_(pref_path,
              reporting_id,
              reporting_ids_count,
              enforcement_level,
              value_type),
      delegate_(delegate) {
}

void TrackedSplitPreference::OnNewValue(
    const base::Value* value,
    PrefHashStoreTransaction* transaction) const {
  const base::DictionaryValue* dict_value = NULL;
  if (value && !value->GetAsDictionary(&dict_value)) {
    NOTREACHED();
    return;
  }
  transaction->StoreSplitHash(pref_path_, dict_value);
}

bool TrackedSplitPreference::EnforceAndReport(
    base::DictionaryValue* pref_store_contents,
    PrefHashStoreTransaction* transaction) const {
  base::DictionaryValue* dict_value = NULL;
  if (!pref_store_contents->GetDictionary(pref_path_, &dict_value) &&
      pref_store_contents->Get(pref_path_, NULL)) {
    // There should be a dictionary or nothing at |pref_path_|.
    NOTREACHED();
    return false;
  }

  std::vector<std::string> invalid_keys;
  PrefHashStoreTransaction::ValueState value_state =
      transaction->CheckSplitValue(pref_path_, dict_value, &invalid_keys);

  if (value_state == PrefHashStoreTransaction::CHANGED)
    helper_.ReportSplitPreferenceChangedCount(invalid_keys.size());

  helper_.ReportValidationResult(value_state);

  TrackedPreferenceHelper::ResetAction reset_action =
      helper_.GetAction(value_state);
  if (delegate_) {
    delegate_->OnSplitPreferenceValidation(pref_path_, dict_value, invalid_keys,
                                           value_state, helper_.IsPersonal());
  }
  helper_.ReportAction(reset_action);

  bool was_reset = false;
  if (reset_action == TrackedPreferenceHelper::DO_RESET) {
    if (value_state == PrefHashStoreTransaction::CHANGED) {
      DCHECK(!invalid_keys.empty());

      for (std::vector<std::string>::const_iterator it = invalid_keys.begin();
           it != invalid_keys.end(); ++it) {
        dict_value->Remove(*it, NULL);
      }
    } else {
      pref_store_contents->RemovePath(pref_path_, NULL);
    }
    was_reset = true;
  }

  if (value_state != PrefHashStoreTransaction::UNCHANGED) {
    // Store the hash for the new value (whether it was reset or not).
    const base::DictionaryValue* new_dict_value = NULL;
    pref_store_contents->GetDictionary(pref_path_, &new_dict_value);
    transaction->StoreSplitHash(pref_path_, new_dict_value);
  }

  return was_reset;
}
