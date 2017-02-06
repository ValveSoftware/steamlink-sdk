// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_prefs/tracked/mock_validation_delegate.h"

MockValidationDelegate::MockValidationDelegate() {
}

MockValidationDelegate::~MockValidationDelegate() {
}

size_t MockValidationDelegate::CountValidationsOfState(
    PrefHashStoreTransaction::ValueState value_state) const {
  size_t count = 0;
  for (size_t i = 0; i < validations_.size(); ++i) {
    if (validations_[i].value_state == value_state)
      ++count;
  }
  return count;
}

const MockValidationDelegate::ValidationEvent*
MockValidationDelegate::GetEventForPath(const std::string& pref_path) const {
  for (size_t i = 0; i < validations_.size(); ++i) {
    if (validations_[i].pref_path == pref_path)
      return &validations_[i];
  }
  return NULL;
}

void MockValidationDelegate::OnAtomicPreferenceValidation(
    const std::string& pref_path,
    const base::Value* value,
    PrefHashStoreTransaction::ValueState value_state,
    bool is_personal) {
  RecordValidation(pref_path, value_state, is_personal,
                   PrefHashFilter::TRACKING_STRATEGY_ATOMIC);
}

void MockValidationDelegate::OnSplitPreferenceValidation(
    const std::string& pref_path,
    const base::DictionaryValue* dict_value,
    const std::vector<std::string>& invalid_keys,
    PrefHashStoreTransaction::ValueState value_state,
    bool is_personal) {
  RecordValidation(pref_path, value_state, is_personal,
                   PrefHashFilter::TRACKING_STRATEGY_SPLIT);
}

void MockValidationDelegate::RecordValidation(
    const std::string& pref_path,
    PrefHashStoreTransaction::ValueState value_state,
    bool is_personal,
    PrefHashFilter::PrefTrackingStrategy strategy) {
  validations_.push_back(
      ValidationEvent(pref_path, value_state, is_personal, strategy));
}
