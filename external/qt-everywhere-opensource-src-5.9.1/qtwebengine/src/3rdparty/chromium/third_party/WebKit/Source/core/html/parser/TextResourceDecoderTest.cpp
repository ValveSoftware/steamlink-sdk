// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/parser/TextResourceDecoder.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

const char kUTF16TextMimeType[] = "text/plain; charset=utf-16";

}  // namespace

TEST(TextResourceDecoderTest, BasicUTF16) {
  std::unique_ptr<TextResourceDecoder> decoder =
      TextResourceDecoder::create(kUTF16TextMimeType);
  WTF::String decoded;

  const unsigned char fooLE[] = {0xff, 0xfe, 0x66, 0x00,
                                 0x6f, 0x00, 0x6f, 0x00};
  decoded =
      decoder->decode(reinterpret_cast<const char*>(fooLE), sizeof(fooLE));
  decoded = decoded + decoder->flush();
  EXPECT_EQ("foo", decoded);

  decoder = TextResourceDecoder::create(kUTF16TextMimeType);
  const unsigned char fooBE[] = {0xfe, 0xff, 0x00, 0x66,
                                 0x00, 0x6f, 0x00, 0x6f};
  decoded =
      decoder->decode(reinterpret_cast<const char*>(fooBE), sizeof(fooBE));
  decoded = decoded + decoder->flush();
  EXPECT_EQ("foo", decoded);
}

TEST(TextResourceDecoderTest, UTF16Pieces) {
  std::unique_ptr<TextResourceDecoder> decoder =
      TextResourceDecoder::create(kUTF16TextMimeType);

  WTF::String decoded;
  const unsigned char foo[] = {0xff, 0xfe, 0x66, 0x00, 0x6f, 0x00, 0x6f, 0x00};
  for (char c : foo)
    decoded = decoded + decoder->decode(reinterpret_cast<const char*>(&c), 1);
  decoded = decoded + decoder->flush();
  EXPECT_EQ("foo", decoded);
}

}  // namespace blink
