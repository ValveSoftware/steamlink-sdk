// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/text/TextEncodingDetector.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/TextEncoding.h"

namespace blink {

TEST(TextResourceDecoderTest, RespectIso2022Jp) {
  // ISO-2022-JP is the only 7-bit encoding defined in WHATWG standard.
  std::string iso2022jp =
      " \x1B"
      "$BKL3$F;F|K\\%O%`%U%!%$%?!<%:$,%=%U%H%P%s%/$H$N%W%l!<%*%U$r@)$7!\"";
  WTF::TextEncoding encoding;
  bool result = detectTextEncoding(iso2022jp.c_str(), iso2022jp.length(),
                                   nullptr, &encoding);
  EXPECT_TRUE(result);
  EXPECT_EQ(WTF::TextEncoding("ISO-2022-JP"), encoding);
}

TEST(TextResourceDecoderTest, Ignore7BitEncoding) {
  // 7-bit encodings except ISO-2022-JP are not supported by WHATWG.
  // They should be detected as plain text (US-ASCII).
  std::string hzGb2312 =
      " ~{\x54\x42\x31\x7D\x37\x22\x55\x39\x35\x3D\x3D\x71~} abc";
  WTF::TextEncoding encoding;
  bool result = detectTextEncoding(hzGb2312.c_str(), hzGb2312.length(), nullptr,
                                   &encoding);
  EXPECT_TRUE(result);
  EXPECT_EQ(WTF::TextEncoding("US-ASCII"), encoding);
}

}  // namespace blink
