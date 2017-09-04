// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_inline_box.h"

#include "core/style/ComputedStyle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class NGInlineBoxForTest : public NGInlineBox {
 public:
  NGInlineBoxForTest(const ComputedStyle* block_style) {
    block_style_ = const_cast<ComputedStyle*>(block_style);
  }
  using NGInlineBox::NGInlineBox;

  String& Text() { return text_content_; }
  Vector<NGLayoutInlineItem>& Items() { return items_; }

  void AppendText(const String& text) {
    unsigned start = text_content_.length();
    text_content_.append(text);
    items_.emplace_back(start, start + text.length(), nullptr);
  }

  void AppendText(const char16_t* text) {
    AppendText(String(reinterpret_cast<const UChar*>(text)));
  }

  void ClearText() {
    text_content_ = String();
    items_.clear();
  }

  void SegmentText() { NGInlineBox::SegmentText(); }
};

class NGInlineBoxTest : public ::testing::Test {};

TEST_F(NGInlineBoxTest, SegmentEmpty) {
  RefPtr<ComputedStyle> style = ComputedStyle::create();
  NGInlineBoxForTest* box = new NGInlineBoxForTest(style.get());
  box->SegmentText();
}

TEST_F(NGInlineBoxTest, SegmentASCII) {
  RefPtr<ComputedStyle> style = ComputedStyle::create();
  NGInlineBoxForTest* box = new NGInlineBoxForTest(style.get());
  box->AppendText("Hello");
  box->SegmentText();
  ASSERT_EQ(1u, box->Items().size());
  NGLayoutInlineItem& item = box->Items()[0];
  EXPECT_EQ(0u, item.StartOffset());
  EXPECT_EQ(5u, item.EndOffset());
  EXPECT_EQ(LTR, item.Direction());
}

TEST_F(NGInlineBoxTest, SegmentHebrew) {
  RefPtr<ComputedStyle> style = ComputedStyle::create();
  NGInlineBoxForTest* box = new NGInlineBoxForTest(style.get());
  box->AppendText(u"\u05E2\u05D1\u05E8\u05D9\u05EA");
  box->SegmentText();
  ASSERT_EQ(1u, box->Items().size());
  NGLayoutInlineItem& item = box->Items()[0];
  EXPECT_EQ(0u, item.StartOffset());
  EXPECT_EQ(5u, item.EndOffset());
  EXPECT_EQ(RTL, item.Direction());
}

TEST_F(NGInlineBoxTest, SegmentSplit1To2) {
  RefPtr<ComputedStyle> style = ComputedStyle::create();
  NGInlineBoxForTest* box = new NGInlineBoxForTest(style.get());
  box->AppendText(u"Hello \u05E2\u05D1\u05E8\u05D9\u05EA");
  box->SegmentText();
  ASSERT_EQ(2u, box->Items().size());
  NGLayoutInlineItem& item = box->Items()[0];
  EXPECT_EQ(0u, item.StartOffset());
  EXPECT_EQ(6u, item.EndOffset());
  EXPECT_EQ(LTR, item.Direction());
  item = box->Items()[1];
  EXPECT_EQ(6u, item.StartOffset());
  EXPECT_EQ(11u, item.EndOffset());
  EXPECT_EQ(RTL, item.Direction());
}

TEST_F(NGInlineBoxTest, SegmentSplit3To4) {
  RefPtr<ComputedStyle> style = ComputedStyle::create();
  NGInlineBoxForTest* box = new NGInlineBoxForTest(style.get());
  box->AppendText("Hel");
  box->AppendText(u"lo \u05E2");
  box->AppendText(u"\u05D1\u05E8\u05D9\u05EA");
  box->SegmentText();
  ASSERT_EQ(4u, box->Items().size());
  NGLayoutInlineItem& item = box->Items()[0];
  EXPECT_EQ(0u, item.StartOffset());
  EXPECT_EQ(3u, item.EndOffset());
  EXPECT_EQ(LTR, item.Direction());
  item = box->Items()[1];
  EXPECT_EQ(3u, item.StartOffset());
  EXPECT_EQ(6u, item.EndOffset());
  EXPECT_EQ(LTR, item.Direction());
  item = box->Items()[2];
  EXPECT_EQ(6u, item.StartOffset());
  EXPECT_EQ(7u, item.EndOffset());
  EXPECT_EQ(RTL, item.Direction());
  item = box->Items()[3];
  EXPECT_EQ(7u, item.StartOffset());
  EXPECT_EQ(11u, item.EndOffset());
  EXPECT_EQ(RTL, item.Direction());
}

}  // namespace

}  // namespace blink
