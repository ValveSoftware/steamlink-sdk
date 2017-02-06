// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_TEST_PASSWORD_STORE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_TEST_PASSWORD_STORE_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/password_manager/core/browser/password_store.h"

namespace password_manager {

// A very simple PasswordStore implementation that keeps all of the passwords
// in memory and does all its manipulations on the main thread. Since this
// is only used for testing, only the parts of the interface that are needed
// for testing have been implemented.
class TestPasswordStore : public PasswordStore {
 public:
  TestPasswordStore();

  typedef std::map<std::string /* signon_realm */,
                   std::vector<autofill::PasswordForm>> PasswordMap;

  const PasswordMap& stored_passwords() const;
  void Clear();

  // Returns true if no passwords are stored in the store. Note that this is not
  // as simple as asking whether stored_passwords().empty(), because the map can
  // have entries of size 0.
  bool IsEmpty() const;

 protected:
  ~TestPasswordStore() override;

  // PasswordStore interface
  PasswordStoreChangeList AddLoginImpl(
      const autofill::PasswordForm& form) override;
  PasswordStoreChangeList UpdateLoginImpl(
      const autofill::PasswordForm& form) override;
  PasswordStoreChangeList RemoveLoginImpl(
      const autofill::PasswordForm& form) override;
  ScopedVector<autofill::PasswordForm> FillMatchingLogins(
      const autofill::PasswordForm& form) override;

  // Unused portions of PasswordStore interface
  void ReportMetricsImpl(const std::string& sync_username,
                         bool custom_passphrase_sync_enabled) override;
  PasswordStoreChangeList RemoveLoginsByURLAndTimeImpl(
      const base::Callback<bool(const GURL&)>& url_filter,
      base::Time begin,
      base::Time end) override;
  PasswordStoreChangeList RemoveLoginsCreatedBetweenImpl(
      base::Time begin,
      base::Time end) override;
  PasswordStoreChangeList RemoveLoginsSyncedBetweenImpl(
      base::Time delete_begin,
      base::Time delete_end) override;
  PasswordStoreChangeList DisableAutoSignInForAllLoginsImpl() override;
  bool RemoveStatisticsCreatedBetweenImpl(base::Time delete_begin,
                                          base::Time delete_end) override;
  bool FillAutofillableLogins(
      ScopedVector<autofill::PasswordForm>* forms) override;
  bool FillBlacklistLogins(
      ScopedVector<autofill::PasswordForm>* forms) override;
  void AddSiteStatsImpl(const InteractionsStats& stats) override;
  void RemoveSiteStatsImpl(const GURL& origin_domain) override;
  std::vector<std::unique_ptr<InteractionsStats>> GetSiteStatsImpl(
      const GURL& origin_domain) override;

 private:
  PasswordMap stored_passwords_;

  DISALLOW_COPY_AND_ASSIGN(TestPasswordStore);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_TEST_PASSWORD_STORE_H_
