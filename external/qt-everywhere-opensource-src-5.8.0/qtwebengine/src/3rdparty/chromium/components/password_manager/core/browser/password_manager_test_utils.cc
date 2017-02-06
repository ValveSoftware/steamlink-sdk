// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_test_utils.h"

#include <algorithm>
#include <ostream>
#include <string>

#include "base/feature_list.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

using autofill::PasswordForm;

namespace password_manager {

namespace {

void GetFeatureOverridesAsCSV(const std::vector<const base::Feature*>& features,
                              std::string* overrides) {
  for (const base::Feature* feature : features) {
    overrides->append(feature->name);
    overrides->push_back(',');
  }
}

}  // namespace

const char kTestingIconUrlSpec[] = "https://accounts.google.com/Icon";
const char kTestingFederationUrlSpec[] = "https://accounts.google.com/login";
const int kTestingDaysAfterPasswordsAreSynced = 1;
const wchar_t kTestingFederatedLoginMarker[] = L"__federated__";

std::unique_ptr<PasswordForm> CreatePasswordFormFromDataForTesting(
    const PasswordFormData& form_data) {
  std::unique_ptr<PasswordForm> form(new PasswordForm());
  form->scheme = form_data.scheme;
  form->preferred = form_data.preferred;
  form->ssl_valid = form_data.ssl_valid;
  form->date_created = base::Time::FromDoubleT(form_data.creation_time);
  form->date_synced =
      form->date_created +
      base::TimeDelta::FromDays(kTestingDaysAfterPasswordsAreSynced);
  if (form_data.signon_realm)
    form->signon_realm = std::string(form_data.signon_realm);
  if (form_data.origin)
    form->origin = GURL(form_data.origin);
  if (form_data.action)
    form->action = GURL(form_data.action);
  if (form_data.submit_element)
    form->submit_element = base::WideToUTF16(form_data.submit_element);
  if (form_data.username_element)
    form->username_element = base::WideToUTF16(form_data.username_element);
  if (form_data.password_element)
    form->password_element = base::WideToUTF16(form_data.password_element);
  if (form_data.username_value) {
    form->username_value = base::WideToUTF16(form_data.username_value);
    form->display_name = form->username_value;
    form->skip_zero_click = true;
    if (form_data.password_value) {
      if (wcscmp(form_data.password_value, kTestingFederatedLoginMarker) == 0)
        form->federation_origin = url::Origin(GURL(kTestingFederationUrlSpec));
      else
        form->password_value = base::WideToUTF16(form_data.password_value);
    }
  } else {
    form->blacklisted_by_user = true;
  }
  form->icon_url = GURL(kTestingIconUrlSpec);
  return form;
}

bool ContainsEqualPasswordFormsUnordered(
    const std::vector<PasswordForm*>& expectations,
    const std::vector<PasswordForm*>& actual_values,
    std::ostream* mismatch_output) {
  std::vector<PasswordForm*> remaining_expectations(expectations.begin(),
                                                    expectations.end());
  bool had_mismatched_actual_form = false;
  for (const PasswordForm* actual : actual_values) {
    auto it_matching_expectation = std::find_if(
        remaining_expectations.begin(), remaining_expectations.end(),
        [actual](PasswordForm* expected) { return *expected == *actual; });
    if (it_matching_expectation != remaining_expectations.end()) {
      // Erase the matched expectation by moving the last element to its place.
      *it_matching_expectation = remaining_expectations.back();
      remaining_expectations.pop_back();
    } else {
      if (mismatch_output) {
        *mismatch_output << std::endl
                         << "Unmatched actual form:" << std::endl
                         << *actual;
      }
      had_mismatched_actual_form = true;
    }
  }

  if (mismatch_output) {
    for (const PasswordForm* remaining_expected_form : remaining_expectations) {
      *mismatch_output << std::endl
                       << "Unmatched expected form:" << std::endl
                       << *remaining_expected_form;
    }
  }

  return !had_mismatched_actual_form && remaining_expectations.empty();
}

void SetFeatures(const std::vector<const base::Feature*>& enable_features,
                 const std::vector<const base::Feature*>& disable_features,
                 std::unique_ptr<base::FeatureList> feature_list) {
  std::string enable_overrides;
  std::string disable_overrides;

  GetFeatureOverridesAsCSV(enable_features, &enable_overrides);
  GetFeatureOverridesAsCSV(disable_features, &disable_overrides);

  base::FeatureList::ClearInstanceForTesting();
  feature_list->InitializeFromCommandLine(enable_overrides, disable_overrides);
  base::FeatureList::SetInstance(std::move(feature_list));
}

MockPasswordStoreObserver::MockPasswordStoreObserver() {}

MockPasswordStoreObserver::~MockPasswordStoreObserver() {}

}  // namespace password_manager
