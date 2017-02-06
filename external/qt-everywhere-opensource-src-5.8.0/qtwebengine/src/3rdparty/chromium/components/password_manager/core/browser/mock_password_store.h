// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_STORE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_STORE_H_

#include <string>
#include <vector>

#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

class MockPasswordStore : public PasswordStore {
 public:
  MockPasswordStore();

  MOCK_METHOD1(RemoveLogin, void(const autofill::PasswordForm&));
  MOCK_METHOD2(GetLogins,
               void(const autofill::PasswordForm&, PasswordStoreConsumer*));
  MOCK_METHOD1(AddLogin, void(const autofill::PasswordForm&));
  MOCK_METHOD1(UpdateLogin, void(const autofill::PasswordForm&));
  MOCK_METHOD2(UpdateLoginWithPrimaryKey,
               void(const autofill::PasswordForm&,
                    const autofill::PasswordForm&));
  MOCK_METHOD2(ReportMetrics, void(const std::string&, bool));
  MOCK_METHOD2(ReportMetricsImpl, void(const std::string&, bool));
  MOCK_METHOD1(AddLoginImpl,
               PasswordStoreChangeList(const autofill::PasswordForm&));
  MOCK_METHOD1(UpdateLoginImpl,
               PasswordStoreChangeList(const autofill::PasswordForm&));
  MOCK_METHOD1(RemoveLoginImpl,
               PasswordStoreChangeList(const autofill::PasswordForm&));
  MOCK_METHOD3(RemoveLoginsByURLAndTimeImpl,
               PasswordStoreChangeList(const base::Callback<bool(const GURL&)>&,
                                       base::Time,
                                       base::Time));
  MOCK_METHOD2(RemoveLoginsCreatedBetweenImpl,
               PasswordStoreChangeList(base::Time, base::Time));
  MOCK_METHOD2(RemoveLoginsSyncedBetweenImpl,
               PasswordStoreChangeList(base::Time, base::Time));
  MOCK_METHOD2(RemoveStatisticsCreatedBetweenImpl,
               bool(base::Time, base::Time));
  MOCK_METHOD0(DisableAutoSignInForAllLoginsImpl, PasswordStoreChangeList());
  ScopedVector<autofill::PasswordForm> FillMatchingLogins(
      const autofill::PasswordForm& form) override {
    return ScopedVector<autofill::PasswordForm>();
  }
  MOCK_METHOD1(FillAutofillableLogins,
               bool(ScopedVector<autofill::PasswordForm>*));
  MOCK_METHOD1(FillBlacklistLogins,
               bool(ScopedVector<autofill::PasswordForm>*));
  MOCK_METHOD1(NotifyLoginsChanged, void(const PasswordStoreChangeList&));
  // GMock doesn't allow to return noncopyable types.
  std::vector<std::unique_ptr<InteractionsStats>> GetSiteStatsImpl(
      const GURL& origin_domain) override;
  MOCK_METHOD1(GetSiteStatsMock, std::vector<InteractionsStats*>(const GURL&));
  MOCK_METHOD1(AddSiteStatsImpl, void(const InteractionsStats&));
  MOCK_METHOD1(RemoveSiteStatsImpl, void(const GURL&));

  PasswordStoreSync* GetSyncInterface() { return this; }

 protected:
  virtual ~MockPasswordStore();
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_STORE_H_
