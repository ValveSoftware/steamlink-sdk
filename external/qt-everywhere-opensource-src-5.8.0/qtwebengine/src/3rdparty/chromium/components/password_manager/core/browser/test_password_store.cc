// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/test_password_store.h"

#include <stddef.h>

#include "base/threading/thread_task_runner_handle.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/psl_matching_helper.h"
#include "components/password_manager/core/browser/statistics_table.h"

namespace password_manager {

TestPasswordStore::TestPasswordStore()
    : PasswordStore(base::ThreadTaskRunnerHandle::Get(),
                    base::ThreadTaskRunnerHandle::Get()) {
}

TestPasswordStore::~TestPasswordStore() {
}

const TestPasswordStore::PasswordMap& TestPasswordStore::stored_passwords()
    const {
  return stored_passwords_;
}

void TestPasswordStore::Clear() {
  stored_passwords_.clear();
}

bool TestPasswordStore::IsEmpty() const {
  // The store is empty, if the sum of all stored passwords across all entries
  // in |stored_passwords_| is 0.
  size_t number_of_passwords = 0u;
  for (PasswordMap::const_iterator it = stored_passwords_.begin();
       !number_of_passwords && it != stored_passwords_.end(); ++it) {
    number_of_passwords += it->second.size();
  }
  return number_of_passwords == 0u;
}

PasswordStoreChangeList TestPasswordStore::AddLoginImpl(
    const autofill::PasswordForm& form) {
  PasswordStoreChangeList changes;
  stored_passwords_[form.signon_realm].push_back(form);
  changes.push_back(PasswordStoreChange(PasswordStoreChange::ADD, form));
  return changes;
}

PasswordStoreChangeList TestPasswordStore::UpdateLoginImpl(
    const autofill::PasswordForm& form) {
  PasswordStoreChangeList changes;
  std::vector<autofill::PasswordForm>& forms =
      stored_passwords_[form.signon_realm];
  for (std::vector<autofill::PasswordForm>::iterator it = forms.begin();
       it != forms.end(); ++it) {
    if (ArePasswordFormUniqueKeyEqual(form, *it)) {
      *it = form;
      changes.push_back(PasswordStoreChange(PasswordStoreChange::UPDATE, form));
    }
  }
  return changes;
}

PasswordStoreChangeList TestPasswordStore::RemoveLoginImpl(
    const autofill::PasswordForm& form) {
  PasswordStoreChangeList changes;
  std::vector<autofill::PasswordForm>& forms =
      stored_passwords_[form.signon_realm];
  std::vector<autofill::PasswordForm>::iterator it = forms.begin();
  while (it != forms.end()) {
    if (ArePasswordFormUniqueKeyEqual(form, *it)) {
      it = forms.erase(it);
      changes.push_back(PasswordStoreChange(PasswordStoreChange::REMOVE, form));
    } else {
      ++it;
    }
  }
  return changes;
}

ScopedVector<autofill::PasswordForm> TestPasswordStore::FillMatchingLogins(
    const autofill::PasswordForm& form) {
  ScopedVector<autofill::PasswordForm> matched_forms;
  for (const auto& elements : stored_passwords_) {
    if (elements.first == form.signon_realm ||
        (form.scheme == autofill::PasswordForm::SCHEME_HTML &&
         password_manager::IsFederatedMatch(elements.first, form.origin))) {
      for (const auto& stored_form : elements.second)
        matched_forms.push_back(new autofill::PasswordForm(stored_form));
    }
  }
  return matched_forms;
}

void TestPasswordStore::ReportMetricsImpl(const std::string& sync_username,
                                          bool custom_passphrase_sync_enabled) {
}

PasswordStoreChangeList TestPasswordStore::RemoveLoginsByURLAndTimeImpl(
    const base::Callback<bool(const GURL&)>& url_filter,
    base::Time begin,
    base::Time end) {
  return PasswordStoreChangeList();
}

PasswordStoreChangeList TestPasswordStore::RemoveLoginsCreatedBetweenImpl(
    base::Time begin,
    base::Time end) {
  return PasswordStoreChangeList();
}

PasswordStoreChangeList TestPasswordStore::RemoveLoginsSyncedBetweenImpl(
    base::Time begin,
    base::Time end) {
  return PasswordStoreChangeList();
}

PasswordStoreChangeList TestPasswordStore::DisableAutoSignInForAllLoginsImpl() {
  return PasswordStoreChangeList();
}

bool TestPasswordStore::RemoveStatisticsCreatedBetweenImpl(
    base::Time delete_begin,
    base::Time delete_end) {
  return false;
}

bool TestPasswordStore::FillAutofillableLogins(
    ScopedVector<autofill::PasswordForm>* forms) {
  for (const auto& forms_for_realm : stored_passwords_) {
    for (const autofill::PasswordForm& form : forms_for_realm.second) {
      if (!form.blacklisted_by_user)
        forms->push_back(new autofill::PasswordForm(form));
    }
  }
  return true;
}

bool TestPasswordStore::FillBlacklistLogins(
    ScopedVector<autofill::PasswordForm>* forms) {
  for (const auto& forms_for_realm : stored_passwords_) {
    for (const autofill::PasswordForm& form : forms_for_realm.second) {
      if (form.blacklisted_by_user)
        forms->push_back(new autofill::PasswordForm(form));
    }
  }
  return true;
}

void TestPasswordStore::AddSiteStatsImpl(const InteractionsStats& stats) {
}

void TestPasswordStore::RemoveSiteStatsImpl(const GURL& origin_domain) {
}

std::vector<std::unique_ptr<InteractionsStats>>
TestPasswordStore::GetSiteStatsImpl(const GURL& origin_domain) {
  return std::vector<std::unique_ptr<InteractionsStats>>();
}

}  // namespace password_manager
