// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <sstream>

#include "base/macros.h"
#include "device/hid/hid_report_descriptor.h"
#include "device/hid/test_report_descriptors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace testing;

namespace device {

class HidReportDescriptorTest : public testing::Test {

 protected:
  void SetUp() override { descriptor_ = NULL; }

  void TearDown() override {
    if (descriptor_) {
      delete descriptor_;
    }
  }

 public:
  void ValidateDetails(
      const std::vector<HidCollectionInfo>& expected_collections,
      const bool expected_has_report_id,
      const size_t expected_max_input_report_size,
      const size_t expected_max_output_report_size,
      const size_t expected_max_feature_report_size,
      const uint8_t* bytes,
      size_t size) {
    descriptor_ =
        new HidReportDescriptor(std::vector<uint8_t>(bytes, bytes + size));

    std::vector<HidCollectionInfo> actual_collections;
    bool actual_has_report_id;
    size_t actual_max_input_report_size;
    size_t actual_max_output_report_size;
    size_t actual_max_feature_report_size;
    descriptor_->GetDetails(&actual_collections,
                            &actual_has_report_id,
                            &actual_max_input_report_size,
                            &actual_max_output_report_size,
                            &actual_max_feature_report_size);

    ASSERT_EQ(expected_collections.size(), actual_collections.size());

    std::vector<HidCollectionInfo>::const_iterator actual_collections_iter =
        actual_collections.begin();
    std::vector<HidCollectionInfo>::const_iterator expected_collections_iter =
        expected_collections.begin();

    while (expected_collections_iter != expected_collections.end() &&
           actual_collections_iter != actual_collections.end()) {
      HidCollectionInfo expected_collection = *expected_collections_iter;
      HidCollectionInfo actual_collection = *actual_collections_iter;

      ASSERT_EQ(expected_collection.usage.usage_page,
                actual_collection.usage.usage_page);
      ASSERT_EQ(expected_collection.usage.usage, actual_collection.usage.usage);
      ASSERT_THAT(actual_collection.report_ids,
                  ContainerEq(expected_collection.report_ids));

      expected_collections_iter++;
      actual_collections_iter++;
    }

    ASSERT_EQ(expected_has_report_id, actual_has_report_id);
    ASSERT_EQ(expected_max_input_report_size, actual_max_input_report_size);
    ASSERT_EQ(expected_max_output_report_size, actual_max_output_report_size);
    ASSERT_EQ(expected_max_feature_report_size, actual_max_feature_report_size);
  }

 private:
  HidReportDescriptor* descriptor_;
};

TEST_F(HidReportDescriptorTest, ValidateDetails_Digitizer) {
  HidCollectionInfo digitizer;
  digitizer.usage = HidUsageAndPage(0x01, HidUsageAndPage::kPageDigitizer);
  digitizer.report_ids.insert(1);
  digitizer.report_ids.insert(2);
  digitizer.report_ids.insert(3);
  HidCollectionInfo expected[] = {digitizer};
  ValidateDetails(
      std::vector<HidCollectionInfo>(expected, expected + arraysize(expected)),
      true, 6, 0, 0, kDigitizer, kDigitizerSize);
}

TEST_F(HidReportDescriptorTest, ValidateDetails_Keyboard) {
  HidCollectionInfo keyboard;
  keyboard.usage = HidUsageAndPage(0x06, HidUsageAndPage::kPageGenericDesktop);
  HidCollectionInfo expected[] = {keyboard};
  ValidateDetails(
      std::vector<HidCollectionInfo>(expected, expected + arraysize(expected)),
      false, 8, 1, 0, kKeyboard, kKeyboardSize);
}

TEST_F(HidReportDescriptorTest, ValidateDetails_Monitor) {
  HidCollectionInfo monitor;
  monitor.usage = HidUsageAndPage(0x01, HidUsageAndPage::kPageMonitor0);
  monitor.report_ids.insert(1);
  monitor.report_ids.insert(2);
  monitor.report_ids.insert(3);
  monitor.report_ids.insert(4);
  monitor.report_ids.insert(5);
  HidCollectionInfo expected[] = {monitor};
  ValidateDetails(
      std::vector<HidCollectionInfo>(expected, expected + arraysize(expected)),
      true, 0, 0, 243, kMonitor, kMonitorSize);
}

TEST_F(HidReportDescriptorTest, ValidateDetails_Mouse) {
  HidCollectionInfo mouse;
  mouse.usage = HidUsageAndPage(0x02, HidUsageAndPage::kPageGenericDesktop);
  HidCollectionInfo expected[] = {mouse};
  ValidateDetails(
      std::vector<HidCollectionInfo>(expected, expected + arraysize(expected)),
      false, 3, 0, 0, kMouse, kMouseSize);
}

TEST_F(HidReportDescriptorTest, ValidateDetails_LogitechUnifyingReceiver) {
  HidCollectionInfo hidpp_short;
  hidpp_short.usage = HidUsageAndPage(0x01, HidUsageAndPage::kPageVendor);
  hidpp_short.report_ids.insert(0x10);
  HidCollectionInfo hidpp_long;
  hidpp_long.usage = HidUsageAndPage(0x02, HidUsageAndPage::kPageVendor);
  hidpp_long.report_ids.insert(0x11);
  HidCollectionInfo hidpp_dj;
  hidpp_dj.usage = HidUsageAndPage(0x04, HidUsageAndPage::kPageVendor);
  hidpp_dj.report_ids.insert(0x20);
  hidpp_dj.report_ids.insert(0x21);

  HidCollectionInfo expected[] = {hidpp_short, hidpp_long, hidpp_dj};
  ValidateDetails(
      std::vector<HidCollectionInfo>(expected, expected + arraysize(expected)),
      true, 31, 31, 0, kLogitechUnifyingReceiver,
      kLogitechUnifyingReceiverSize);
}

}  // namespace device
