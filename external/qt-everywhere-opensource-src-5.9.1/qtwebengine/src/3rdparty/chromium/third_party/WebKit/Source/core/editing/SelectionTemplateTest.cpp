// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/SelectionTemplate.h"

#include "core/editing/EditingTestBase.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class SelectionTest : public EditingTestBase {};

TEST_F(SelectionTest, defaultConstructor) {
  SelectionInDOMTree selection;

  EXPECT_EQ(TextAffinity::Downstream, selection.affinity());
  EXPECT_EQ(CharacterGranularity, selection.granularity());
  EXPECT_FALSE(selection.hasTrailingWhitespace());
  EXPECT_FALSE(selection.isDirectional());
  EXPECT_TRUE(selection.isNone());
  EXPECT_EQ(Position(), selection.base());
  EXPECT_EQ(Position(), selection.extent());
}

TEST_F(SelectionTest, caret) {
  setBodyContent("<div id='sample'>abcdef</div>");

  Element* sample = document().getElementById("sample");
  Position position(Position(sample->firstChild(), 2));
  SelectionInDOMTree::Builder builder;
  builder.collapse(position);
  const SelectionInDOMTree& selection = builder.build();

  EXPECT_EQ(TextAffinity::Downstream, selection.affinity());
  EXPECT_EQ(CharacterGranularity, selection.granularity());
  EXPECT_FALSE(selection.hasTrailingWhitespace());
  EXPECT_FALSE(selection.isDirectional());
  EXPECT_FALSE(selection.isNone());
  EXPECT_EQ(position, selection.base());
  EXPECT_EQ(position, selection.extent());
}

TEST_F(SelectionTest, range) {
  setBodyContent("<div id='sample'>abcdef</div>");

  Element* sample = document().getElementById("sample");
  Position base(Position(sample->firstChild(), 2));
  Position extent(Position(sample->firstChild(), 4));
  SelectionInDOMTree::Builder builder;
  builder.collapse(base);
  builder.extend(extent);
  const SelectionInDOMTree& selection = builder.build();

  EXPECT_EQ(TextAffinity::Downstream, selection.affinity());
  EXPECT_EQ(CharacterGranularity, selection.granularity());
  EXPECT_FALSE(selection.hasTrailingWhitespace());
  EXPECT_FALSE(selection.isDirectional());
  EXPECT_FALSE(selection.isNone());
  EXPECT_EQ(base, selection.base());
  EXPECT_EQ(extent, selection.extent());
}

}  // namespace blink
