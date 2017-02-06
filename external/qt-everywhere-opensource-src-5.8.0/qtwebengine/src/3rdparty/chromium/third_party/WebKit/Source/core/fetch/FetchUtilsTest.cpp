// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fetch/FetchUtils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/WTFString.h"

namespace blink {

namespace {

TEST(FetchUtilsTest, NormalizeHeaderValue)
{
    EXPECT_EQ("t", FetchUtils::normalizeHeaderValue(" t"));
    EXPECT_EQ("t", FetchUtils::normalizeHeaderValue("t "));
    EXPECT_EQ("t", FetchUtils::normalizeHeaderValue(" t "));
    EXPECT_EQ("test", FetchUtils::normalizeHeaderValue("test\r"));
    EXPECT_EQ("test", FetchUtils::normalizeHeaderValue("test\n"));
    EXPECT_EQ("test", FetchUtils::normalizeHeaderValue("test\r\n"));
    EXPECT_EQ("test", FetchUtils::normalizeHeaderValue("test\t"));
    EXPECT_EQ("t t", FetchUtils::normalizeHeaderValue("t t"));
    EXPECT_EQ("t\tt", FetchUtils::normalizeHeaderValue("t\tt"));
    EXPECT_EQ("t\rt", FetchUtils::normalizeHeaderValue("t\rt"));
    EXPECT_EQ("t\nt", FetchUtils::normalizeHeaderValue("t\nt"));
    EXPECT_EQ("t\r\nt", FetchUtils::normalizeHeaderValue("t\r\nt"));
    EXPECT_EQ("test", FetchUtils::normalizeHeaderValue("\rtest"));
    EXPECT_EQ("test", FetchUtils::normalizeHeaderValue("\ntest"));
    EXPECT_EQ("test", FetchUtils::normalizeHeaderValue("\r\ntest"));
    EXPECT_EQ("test", FetchUtils::normalizeHeaderValue("\ttest"));
    EXPECT_EQ("", FetchUtils::normalizeHeaderValue(""));
    EXPECT_EQ("", FetchUtils::normalizeHeaderValue(" "));
    EXPECT_EQ("", FetchUtils::normalizeHeaderValue("\r\n\r\n\r\n"));
    EXPECT_EQ("\xd0\xa1", FetchUtils::normalizeHeaderValue("\xd0\xa1"));
    EXPECT_EQ("test", FetchUtils::normalizeHeaderValue("test"));
}

} // namespace

} // namespace blink
