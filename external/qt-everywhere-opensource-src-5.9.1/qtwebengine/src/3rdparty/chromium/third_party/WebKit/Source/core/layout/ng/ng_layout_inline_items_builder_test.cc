// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_layout_inline_items_builder.h"
#include "core/layout/LayoutInline.h"
#include "core/layout/ng/ng_inline_box.h"
#include "core/style/ComputedStyle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class NGLayoutInlineItemsBuilderTest : public ::testing::Test {};

TEST_F(NGLayoutInlineItemsBuilderTest, Empty) {
  Vector<NGLayoutInlineItem> items;
  NGLayoutInlineItemsBuilder builder(&items);
  RefPtr<ComputedStyle> block_style(ComputedStyle::create());
  builder.EnterBlock(block_style.get());
  builder.ExitBlock();

  EXPECT_EQ("", builder.ToString());
}

TEST_F(NGLayoutInlineItemsBuilderTest, BidiBlockOverride) {
  Vector<NGLayoutInlineItem> items;
  NGLayoutInlineItemsBuilder builder(&items);
  RefPtr<ComputedStyle> block_style(ComputedStyle::create());
  block_style->setUnicodeBidi(Override);
  block_style->setDirection(RTL);
  builder.EnterBlock(block_style.get());
  builder.Append("Hello", nullptr);
  builder.ExitBlock();

  // Expected control characters as defined in:
  // https://drafts.csswg.org/css-writing-modes-3/#bidi-control-codes-injection-table
  EXPECT_EQ(String(reinterpret_cast<const UChar*>(u"\u202E"
                                                  u"Hello"
                                                  u"\u202C")),
            builder.ToString());
}

static std::unique_ptr<LayoutInline> createLayoutInline(
    void (*initialize_style)(ComputedStyle*)) {
  RefPtr<ComputedStyle> style(ComputedStyle::create());
  initialize_style(style.get());
  std::unique_ptr<LayoutInline> node = makeUnique<LayoutInline>(nullptr);
  node->setStyleInternal(std::move(style));
  return node;
}

TEST_F(NGLayoutInlineItemsBuilderTest, BidiIsolate) {
  Vector<NGLayoutInlineItem> items;
  NGLayoutInlineItemsBuilder builder(&items);
  builder.Append("Hello ", nullptr);
  std::unique_ptr<LayoutInline> isolateRTL(
      createLayoutInline([](ComputedStyle* style) {
        style->setUnicodeBidi(Isolate);
        style->setDirection(RTL);
      }));
  builder.EnterInline(isolateRTL.get());
  builder.Append(
      reinterpret_cast<const UChar*>(u"\u05E2\u05D1\u05E8\u05D9\u05EA"),
      nullptr);
  builder.ExitInline(isolateRTL.get());
  builder.Append(" World", nullptr);

  // Expected control characters as defined in:
  // https://drafts.csswg.org/css-writing-modes-3/#bidi-control-codes-injection-table
  EXPECT_EQ(
      String(reinterpret_cast<const UChar*>(u"Hello "
                                            u"\u2067"
                                            u"\u05E2\u05D1\u05E8\u05D9\u05EA"
                                            u"\u2069"
                                            u" World")),
      builder.ToString());
}

TEST_F(NGLayoutInlineItemsBuilderTest, BidiIsolateOverride) {
  Vector<NGLayoutInlineItem> items;
  NGLayoutInlineItemsBuilder builder(&items);
  builder.Append("Hello ", nullptr);
  std::unique_ptr<LayoutInline> isolateOverrideRTL(
      createLayoutInline([](ComputedStyle* style) {
        style->setUnicodeBidi(IsolateOverride);
        style->setDirection(RTL);
      }));
  builder.EnterInline(isolateOverrideRTL.get());
  builder.Append(
      reinterpret_cast<const UChar*>(u"\u05E2\u05D1\u05E8\u05D9\u05EA"),
      nullptr);
  builder.ExitInline(isolateOverrideRTL.get());
  builder.Append(" World", nullptr);

  // Expected control characters as defined in:
  // https://drafts.csswg.org/css-writing-modes-3/#bidi-control-codes-injection-table
  EXPECT_EQ(
      String(reinterpret_cast<const UChar*>(u"Hello "
                                            u"\u2068\u202E"
                                            u"\u05E2\u05D1\u05E8\u05D9\u05EA"
                                            u"\u202C\u2069"
                                            u" World")),
      builder.ToString());
}

}  // namespace

}  // namespace blink
