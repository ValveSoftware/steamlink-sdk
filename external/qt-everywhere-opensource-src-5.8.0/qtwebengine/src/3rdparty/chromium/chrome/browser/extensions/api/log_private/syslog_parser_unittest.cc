// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/memory/linked_ptr.h"
#include "chrome/browser/extensions/api/log_private/filter_handler.h"
#include "chrome/browser/extensions/api/log_private/log_parser.h"
#include "chrome/browser/extensions/api/log_private/syslog_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

const char kKernelLogEntry[] =
    "2014-08-18T14:04:58.606132-07:00 kernel: [269374.012690] "
    "cfg80211: World regulatory domain updated:";

const char kShillLogEntry[] =
    "2014-08-15T11:20:24.575058-07:00 shill[1018]: "
    "[INFO:service.cc(290)] Disconnecting from service 32: Unload";

const char kWpaSupplicantLogEntry[] =
    "2014-08-15T12:36:06.137021-07:00 wpa_supplicant[818]: "
    "nl80211: Received scan results (0 BSSes)";

}  // namespace

class ExtensionSyslogParserTest : public testing::Test {
};

TEST_F(ExtensionSyslogParserTest, ParseLog) {
  std::vector<api::log_private::LogEntry> output;
  api::log_private::Filter filter;
  FilterHandler filter_handler(filter);
  SyslogParser p;

  // Test kernel log
  p.Parse(kKernelLogEntry, &output, &filter_handler);
  ASSERT_EQ(1u, output.size());
  EXPECT_EQ("unknown", output[0].level);
  EXPECT_EQ("kernel", output[0].process);
  EXPECT_EQ("unknown", output[0].process_id);
  EXPECT_EQ(kKernelLogEntry, output[0].full_entry);
  EXPECT_DOUBLE_EQ(1408395898606.132, output[0].timestamp);

  // Test shill log
  p.Parse(kShillLogEntry, &output, &filter_handler);
  ASSERT_EQ(2u, output.size());
  EXPECT_EQ("info", output[1].level);
  EXPECT_EQ("shill", output[1].process);
  EXPECT_EQ("1018", output[1].process_id);
  EXPECT_EQ(kShillLogEntry, output[1].full_entry);
  EXPECT_DOUBLE_EQ(1408126824575.058, output[1].timestamp);

  // Test WpaSupplicant log
  p.Parse(kWpaSupplicantLogEntry, &output, &filter_handler);
  ASSERT_EQ(3u, output.size());
  EXPECT_EQ("unknown", output[2].level);
  EXPECT_EQ("wpa_supplicant", output[2].process);
  EXPECT_EQ("818", output[2].process_id);
  EXPECT_EQ(kWpaSupplicantLogEntry, output[2].full_entry);
  EXPECT_DOUBLE_EQ(1408131366137.021, output[2].timestamp);
}

}  // namespace extensions
