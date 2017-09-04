// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/counters/passwords_counter.h"

#include "components/browsing_data/core/pref_names.h"
#include "components/password_manager/core/browser/password_store.h"

namespace browsing_data {

PasswordsCounter::PasswordsCounter(
    scoped_refptr<password_manager::PasswordStore> store) : store_(store) {}

PasswordsCounter::~PasswordsCounter() {
  store_->RemoveObserver(this);
}

void PasswordsCounter::OnInitialized() {
  DCHECK(store_);
  store_->AddObserver(this);
}

const char* PasswordsCounter::GetPrefName() const {
  return browsing_data::prefs::kDeletePasswords;
}

void PasswordsCounter::Count() {
  cancelable_task_tracker()->TryCancelAll();
  // TODO(msramek): We don't actually need the logins themselves, just their
  // count. Consider implementing |PasswordStore::CountAutofillableLogins|.
  // This custom request should also allow us to specify the time range, so that
  // we can use it to filter the login creation date in the database.
  store_->GetAutofillableLogins(this);
}

void PasswordsCounter::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<autofill::PasswordForm>> results) {
  base::Time start = GetPeriodStart();
  ReportResult(std::count_if(
      results.begin(), results.end(),
      [start](const std::unique_ptr<autofill::PasswordForm>& form) {
        return form->date_created >= start;
      }));
}

void PasswordsCounter::OnLoginsChanged(
    const password_manager::PasswordStoreChangeList& changes) {
  Restart();
}

}  // namespace browsing_data
