// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/TextResourceDecoderBuilder.h"

#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

static const WTF::TextEncoding defaultEncodingForURL(const char* url) {
  std::unique_ptr<DummyPageHolder> pageHolder =
      DummyPageHolder::create(IntSize(0, 0));
  Document& document = pageHolder->document();
  document.setURL(KURL(KURL(), url));
  TextResourceDecoderBuilder decoderBuilder("text/html", nullAtom);
  return decoderBuilder.buildFor(&document)->encoding();
}

TEST(TextResourceDecoderBuilderTest, defaultEncodingComesFromTopLevelDomain) {
  EXPECT_EQ(WTF::TextEncoding("Shift_JIS"),
            defaultEncodingForURL("http://tsubotaa.la.coocan.jp"));
  EXPECT_EQ(WTF::TextEncoding("windows-1251"),
            defaultEncodingForURL("http://udarenieru.ru/index.php"));
}

TEST(TextResourceDecoderBuilderTest,
     NoCountryDomainURLDefaultsToLatin1Encoding) {
  // Latin1 encoding is set in |TextResourceDecoder::defaultEncoding()|.
  EXPECT_EQ(WTF::Latin1Encoding(),
            defaultEncodingForURL("http://arstechnica.com/about-us"));
}

}  // namespace blink
