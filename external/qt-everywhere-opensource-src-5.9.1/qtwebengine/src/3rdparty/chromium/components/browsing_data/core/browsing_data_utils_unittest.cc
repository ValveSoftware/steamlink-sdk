// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/browsing_data_utils.h"

#include <string>

#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/browsing_data/core/counters/autofill_counter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FakeWebDataService : public autofill::AutofillWebDataService {
 public:
  FakeWebDataService()
      : AutofillWebDataService(base::ThreadTaskRunnerHandle::Get(),
                               base::ThreadTaskRunnerHandle::Get()) {}

 protected:
  ~FakeWebDataService() override {}
};

}  // namespace

class BrowsingDataUtilsTest : public testing::Test {
 public:
  BrowsingDataUtilsTest() {}
  ~BrowsingDataUtilsTest() override {}

 private:
  base::MessageLoop loop_;
};

// Tests the complex output of the Autofill counter.
TEST_F(BrowsingDataUtilsTest, AutofillCounterResult) {
  browsing_data::AutofillCounter counter(
      scoped_refptr<FakeWebDataService>(new FakeWebDataService()));

  // Test all configurations of zero and nonzero partial results for datatypes.
  // Test singular and plural for each datatype.
  const struct TestCase {
    int num_credit_cards;
    int num_addresses;
    int num_suggestions;
    std::string expected_output;
  } kTestCases[] = {
      {0, 0, 0, "none"},
      {1, 0, 0, "1 credit card"},
      {0, 5, 0, "5 addresses"},
      {0, 0, 1, "1 suggestion"},
      {0, 0, 2, "2 suggestions"},
      {4, 7, 0, "4 credit cards, 7 addresses"},
      {3, 0, 9, "3 credit cards, 9 other suggestions"},
      {0, 1, 1, "1 address, 1 other suggestion"},
      {9, 6, 3, "9 credit cards, 6 addresses, 3 others"},
      {4, 2, 1, "4 credit cards, 2 addresses, 1 other"},
  };

  for (const TestCase& test_case : kTestCases) {
    browsing_data::AutofillCounter::AutofillResult result(
        &counter, test_case.num_suggestions, test_case.num_credit_cards,
        test_case.num_addresses);

    SCOPED_TRACE(
        base::StringPrintf("Test params: %d credit card(s), "
                           "%d address(es), %d suggestion(s).",
                           test_case.num_credit_cards, test_case.num_addresses,
                           test_case.num_suggestions));

    base::string16 output = browsing_data::GetCounterTextFromResult(&result);
    EXPECT_EQ(output, base::ASCIIToUTF16(test_case.expected_output));
  }
}
