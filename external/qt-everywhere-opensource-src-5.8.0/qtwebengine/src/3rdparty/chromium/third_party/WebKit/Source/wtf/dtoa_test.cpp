// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wtf/dtoa.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {

TEST(DtoaTest, TestNumberToFixedPrecisionString)
{
    NumberToStringBuffer buffer;

    // There should be no trailing decimal or zeros.
    numberToFixedPrecisionString(0.0, 6, buffer, true);
    EXPECT_STREQ("0", buffer);

    // Up to 6 leading zeros.
    numberToFixedPrecisionString(0.00000123123123, 6, buffer, true);
    EXPECT_STREQ("0.00000123123", buffer);

    numberToFixedPrecisionString(0.000000123123123, 6, buffer, true);
    EXPECT_STREQ("1.23123e-7", buffer);

    // Up to 6 places before the decimal.
    numberToFixedPrecisionString(123123.123, 6, buffer, true);
    EXPECT_STREQ("123123", buffer);

    numberToFixedPrecisionString(1231231.23, 6, buffer, true);
    EXPECT_STREQ("1.23123e+6", buffer);

    // Don't strip trailing zeros in exponents.
    // http://crbug.com/545711
    numberToFixedPrecisionString(0.000000000123123, 6, buffer, true);
    EXPECT_STREQ("1.23123e-10", buffer);

    // FIXME: Trailing zeros before exponents should be stripped.
    numberToFixedPrecisionString(0.0000000001, 6, buffer, true);
    EXPECT_STREQ("1.00000e-10", buffer);
}

TEST(DtoaTest, TestNumberToFixedPrecisionString_DontTruncateTrailingZeros)
{
    NumberToStringBuffer buffer;

    // There should be a trailing decimal and zeros.
    numberToFixedPrecisionString(0.0, 6, buffer, false);
    EXPECT_STREQ("0.00000", buffer);
}

} // namespace WTF
