// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_AFFILIATED_MATCH_HELPER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_AFFILIATED_MATCH_HELPER_H_

#include "base/macros.h"
#include "components/password_manager/core/browser/affiliated_match_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
struct PasswordForm;
}

namespace password_manager {

class MockAffiliatedMatchHelper : public AffiliatedMatchHelper {
 public:
  MockAffiliatedMatchHelper();
  ~MockAffiliatedMatchHelper() override;

  // Expects GetAffiliatedAndroidRealms() to be called with the
  // |expected_observed_form|, and will cause the result callback supplied to
  // GetAffiliatedAndroidRealms() to be invoked with |results_to_return|.
  void ExpectCallToGetAffiliatedAndroidRealms(
      const autofill::PasswordForm& expected_observed_form,
      const std::vector<std::string>& results_to_return);

  // Expects GetAffiliatedWebRealms() to be called with the
  // |expected_android_form|, and will cause the result callback supplied to
  // GetAffiliatedWebRealms() to be invoked with |results_to_return|.
  void ExpectCallToGetAffiliatedWebRealms(
      const autofill::PasswordForm& expected_android_form,
      const std::vector<std::string>& results_to_return);

  void ExpectCallToInjectAffiliatedWebRealms(
      const std::vector<std::string>& results_to_inject);

 private:
  MOCK_METHOD1(OnGetAffiliatedAndroidRealmsCalled,
               std::vector<std::string>(const autofill::PasswordForm&));
  MOCK_METHOD1(OnGetAffiliatedWebRealmsCalled,
               std::vector<std::string>(const autofill::PasswordForm&));
  MOCK_METHOD0(OnInjectAffiliatedWebRealmsCalled, std::vector<std::string>());

  void GetAffiliatedAndroidRealms(
      const autofill::PasswordForm& observed_form,
      const AffiliatedRealmsCallback& result_callback) override;
  void GetAffiliatedWebRealms(
      const autofill::PasswordForm& android_form,
      const AffiliatedRealmsCallback& result_callback) override;

  void InjectAffiliatedWebRealms(
      ScopedVector<autofill::PasswordForm> forms,
      const PasswordFormsCallback& result_callback) override;

  DISALLOW_COPY_AND_ASSIGN(MockAffiliatedMatchHelper);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_AFFILIATED_MATCH_HELPER_H_
