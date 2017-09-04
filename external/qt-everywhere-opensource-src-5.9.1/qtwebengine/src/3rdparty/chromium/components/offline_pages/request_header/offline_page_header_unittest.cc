// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/request_header/offline_page_header.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

class OfflinePageHeaderTest : public testing::Test {
 public:
  OfflinePageHeaderTest() {}
  ~OfflinePageHeaderTest() override {}

  bool ParseFromHeaderValue(const std::string& header_value,
                            bool* need_to_persist,
                            OfflinePageHeader::Reason* reason,
                            std::string* id) {
    OfflinePageHeader header(header_value);
    *need_to_persist = header.need_to_persist;
    *reason = header.reason;
    *id = header.id;
    return !header.did_fail_parsing_for_test;
  }
};

TEST_F(OfflinePageHeaderTest, Parse) {
  bool need_to_persist;
  OfflinePageHeader::Reason reason;
  std::string id;

  EXPECT_FALSE(ParseFromHeaderValue("", &need_to_persist, &reason, &id));
  EXPECT_FALSE(ParseFromHeaderValue("   ", &need_to_persist, &reason, &id));
  EXPECT_FALSE(ParseFromHeaderValue(" , ", &need_to_persist, &reason, &id));
  EXPECT_FALSE(ParseFromHeaderValue("reason", &need_to_persist, &reason, &id));
  EXPECT_FALSE(ParseFromHeaderValue("a b c", &need_to_persist, &reason, &id));

  EXPECT_FALSE(
      ParseFromHeaderValue("persist=aa", &need_to_persist, &reason, &id));

  EXPECT_TRUE(
      ParseFromHeaderValue("persist=1", &need_to_persist, &reason, &id));
  EXPECT_TRUE(need_to_persist);

  EXPECT_TRUE(
      ParseFromHeaderValue("persist=0", &need_to_persist, &reason, &id));
  EXPECT_FALSE(need_to_persist);
  EXPECT_EQ(OfflinePageHeader::Reason::NONE, reason);
  EXPECT_EQ("", id);

  EXPECT_FALSE(
      ParseFromHeaderValue("reason=foo", &need_to_persist, &reason, &id));

  EXPECT_TRUE(
      ParseFromHeaderValue("reason=error", &need_to_persist, &reason, &id));
  EXPECT_FALSE(need_to_persist);
  EXPECT_EQ(OfflinePageHeader::Reason::NET_ERROR, reason);
  EXPECT_EQ("", id);

  EXPECT_TRUE(ParseFromHeaderValue("id=a1b2", &need_to_persist, &reason, &id));
  EXPECT_FALSE(need_to_persist);
  EXPECT_EQ(OfflinePageHeader::Reason::NONE, reason);
  EXPECT_EQ("a1b2", id);

  EXPECT_TRUE(ParseFromHeaderValue("persist=1 reason=download id=a1b2",
      &need_to_persist, &reason, &id));
  EXPECT_TRUE(need_to_persist);
  EXPECT_EQ(OfflinePageHeader::Reason::DOWNLOAD, reason);
  EXPECT_EQ("a1b2", id);
}

TEST_F(OfflinePageHeaderTest, ToString) {
  OfflinePageHeader header;
  header.need_to_persist = true;
  header.reason = OfflinePageHeader::Reason::DOWNLOAD;
  header.id = "a1b2";
  EXPECT_EQ("X-Chrome-offline: persist=1 reason=download id=a1b2",
            header.GetCompleteHeaderString());
}

}  // namespace offline_pages
