// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/cryptotoken_private/cryptotoken_private_api.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

using namespace api::cryptotoken_private;

class CryptoTokenPrivateApiTest : public extensions::ExtensionApiUnittest {
 public:
  CryptoTokenPrivateApiTest() {}
  ~CryptoTokenPrivateApiTest() override {}

 protected:
  bool GetSingleBooleanResult(
      UIThreadExtensionFunction* function, bool* result) {
    const base::ListValue* result_list = function->GetResultList();
    if (!result_list) {
      LOG(ERROR) << "Function has no result list.";
      return false;
    }

    if (result_list->GetSize() != 1u) {
      LOG(ERROR) << "Invalid number of results.";
      return false;
    }

    if (!result_list->GetBoolean(0, result)) {
      LOG(ERROR) << "Result is not boolean.";
      return false;
    }
    return true;
  }

  bool GetCanOriginAssertAppIdResult(const std::string& origin,
                                     const std::string& appId) {
    scoped_refptr<api::CryptotokenPrivateCanOriginAssertAppIdFunction> function(
        new api::CryptotokenPrivateCanOriginAssertAppIdFunction());
    function->set_has_callback(true);

    std::unique_ptr<base::ListValue> args(new base::ListValue);
    args->AppendString(origin);
    args->AppendString(appId);

    extension_function_test_utils::RunFunction(
        function.get(), std::move(args), browser(),
        extension_function_test_utils::NONE);

    bool result;
    GetSingleBooleanResult(function.get(), &result);
    return result;
  }
};

TEST_F(CryptoTokenPrivateApiTest, CanOriginAssertAppId) {
  std::string origin1("https://www.example.com");

  EXPECT_TRUE(GetCanOriginAssertAppIdResult(origin1, origin1));

  std::string same_origin_appid("https://www.example.com/appId");
  EXPECT_TRUE(GetCanOriginAssertAppIdResult(origin1, same_origin_appid));
  std::string same_etld_plus_one_appid("https://appid.example.com/appId");
  EXPECT_TRUE(GetCanOriginAssertAppIdResult(origin1, same_etld_plus_one_appid));
  std::string different_etld_plus_one_appid("https://www.different.com/appId");
  EXPECT_FALSE(GetCanOriginAssertAppIdResult(origin1,
                                             different_etld_plus_one_appid));

  // For legacy purposes, google.com is allowed to use certain appIds hosted at
  // gstatic.com.
  // TODO(juanlang): remove once the legacy constraints are removed.
  std::string google_origin("https://accounts.google.com");
  std::string gstatic_appid("https://www.gstatic.com/securitykey/origins.json");
  EXPECT_TRUE(GetCanOriginAssertAppIdResult(google_origin, gstatic_appid));
  // Not all gstatic urls are allowed, just those specifically whitelisted.
  std::string gstatic_otherurl("https://www.gstatic.com/foobar");
  EXPECT_FALSE(GetCanOriginAssertAppIdResult(google_origin, gstatic_otherurl));
}

}  // namespace

}  // namespace extensions
