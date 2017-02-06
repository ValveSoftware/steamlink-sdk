// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "components/autofill/core/browser/payments/payments_service_url.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace autofill {
namespace payments {

TEST(PaymentsServiceSandboxUrl, CheckSandboxUrls) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kWalletServiceUseSandbox, "1");

  EXPECT_EQ("https://wallet-web.sandbox.google.com/manage/w/1/paymentMethods",
            GetManageInstrumentsUrl(1).spec());
  EXPECT_EQ("https://wallet-web.sandbox.google.com/manage/w/1/settings/"
            "addresses",
            GetManageAddressesUrl(1).spec());
}

TEST(PaymentsServiceSandboxUrl, CheckProdUrls) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kWalletServiceUseSandbox, "0");

  EXPECT_EQ("https://wallet.google.com/manage/w/1/paymentMethods",
            GetManageInstrumentsUrl(1).spec());
  EXPECT_EQ("https://wallet.google.com/manage/w/1/settings/addresses",
            GetManageAddressesUrl(1).spec());
}

}  // namespace payments
}  // namespace autofill
