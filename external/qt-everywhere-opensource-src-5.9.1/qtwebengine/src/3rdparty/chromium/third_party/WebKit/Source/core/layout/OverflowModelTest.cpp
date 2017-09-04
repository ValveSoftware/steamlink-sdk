/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/layout/OverflowModel.h"

#include "platform/geometry/LayoutRect.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

LayoutRect initialLayoutOverflow() {
  return LayoutRect(10, 10, 80, 80);
}

LayoutRect initialVisualOverflow() {
  return LayoutRect(0, 0, 100, 100);
}

class SimpleOverflowModelTest : public testing::Test {
 protected:
  SimpleOverflowModelTest()
      : m_overflow(initialLayoutOverflow(), initialVisualOverflow()) {}
  SimpleOverflowModel m_overflow;
};

TEST_F(SimpleOverflowModelTest, InitialOverflowRects) {
  EXPECT_EQ(initialLayoutOverflow(), m_overflow.layoutOverflowRect());
  EXPECT_EQ(initialVisualOverflow(), m_overflow.visualOverflowRect());
}

TEST_F(SimpleOverflowModelTest, AddLayoutOverflowOutsideExpandsRect) {
  m_overflow.addLayoutOverflow(LayoutRect(0, 10, 30, 10));
  EXPECT_EQ(LayoutRect(0, 10, 90, 80), m_overflow.layoutOverflowRect());
}

TEST_F(SimpleOverflowModelTest, AddLayoutOverflowInsideDoesNotAffectRect) {
  m_overflow.addLayoutOverflow(LayoutRect(50, 50, 10, 20));
  EXPECT_EQ(initialLayoutOverflow(), m_overflow.layoutOverflowRect());
}

TEST_F(SimpleOverflowModelTest, AddLayoutOverflowEmpty) {
  // This test documents the existing behavior so that we are aware when/if
  // it changes. It would also be reasonable for addLayoutOverflow to be
  // a no-op in this situation.
  m_overflow.addLayoutOverflow(LayoutRect(200, 200, 0, 0));
  EXPECT_EQ(LayoutRect(10, 10, 190, 190), m_overflow.layoutOverflowRect());
}

TEST_F(SimpleOverflowModelTest, AddLayoutOverflowDoesNotAffectVisualOverflow) {
  m_overflow.addLayoutOverflow(LayoutRect(300, 300, 300, 300));
  EXPECT_EQ(initialVisualOverflow(), m_overflow.visualOverflowRect());
}

TEST_F(SimpleOverflowModelTest, AddVisualOverflowOutsideExpandsRect) {
  m_overflow.addVisualOverflow(LayoutRect(150, -50, 10, 10));
  EXPECT_EQ(LayoutRect(0, -50, 160, 150), m_overflow.visualOverflowRect());
}

TEST_F(SimpleOverflowModelTest, AddVisualOverflowInsideDoesNotAffectRect) {
  m_overflow.addVisualOverflow(LayoutRect(0, 10, 90, 90));
  EXPECT_EQ(initialVisualOverflow(), m_overflow.visualOverflowRect());
}

TEST_F(SimpleOverflowModelTest, AddVisualOverflowEmpty) {
  m_overflow.setVisualOverflow(LayoutRect(0, 0, 600, 0));
  m_overflow.addVisualOverflow(LayoutRect(100, -50, 100, 100));
  m_overflow.addVisualOverflow(LayoutRect(300, 300, 0, 10000));
  EXPECT_EQ(LayoutRect(100, -50, 100, 100), m_overflow.visualOverflowRect());
}

TEST_F(SimpleOverflowModelTest, AddVisualOverflowDoesNotAffectLayoutOverflow) {
  m_overflow.addVisualOverflow(LayoutRect(300, 300, 300, 300));
  EXPECT_EQ(initialLayoutOverflow(), m_overflow.layoutOverflowRect());
}

TEST_F(SimpleOverflowModelTest, MoveAffectsLayoutOverflow) {
  m_overflow.move(LayoutUnit(500), LayoutUnit(100));
  EXPECT_EQ(LayoutRect(510, 110, 80, 80), m_overflow.layoutOverflowRect());
}

TEST_F(SimpleOverflowModelTest, MoveAffectsVisualOverflow) {
  m_overflow.move(LayoutUnit(500), LayoutUnit(100));
  EXPECT_EQ(LayoutRect(500, 100, 100, 100), m_overflow.visualOverflowRect());
}

class BoxOverflowModelTest : public testing::Test {
 protected:
  BoxOverflowModelTest()
      : m_overflow(initialLayoutOverflow(), initialVisualOverflow()) {}
  BoxOverflowModel m_overflow;
};

TEST_F(BoxOverflowModelTest, InitialOverflowRects) {
  EXPECT_EQ(initialLayoutOverflow(), m_overflow.layoutOverflowRect());
  EXPECT_EQ(initialVisualOverflow(), m_overflow.selfVisualOverflowRect());
  EXPECT_TRUE(m_overflow.contentsVisualOverflowRect().isEmpty());
}

TEST_F(BoxOverflowModelTest, AddLayoutOverflowOutsideExpandsRect) {
  m_overflow.addLayoutOverflow(LayoutRect(0, 10, 30, 10));
  EXPECT_EQ(LayoutRect(0, 10, 90, 80), m_overflow.layoutOverflowRect());
}

TEST_F(BoxOverflowModelTest, AddLayoutOverflowInsideDoesNotAffectRect) {
  m_overflow.addLayoutOverflow(LayoutRect(50, 50, 10, 20));
  EXPECT_EQ(initialLayoutOverflow(), m_overflow.layoutOverflowRect());
}

TEST_F(BoxOverflowModelTest, AddLayoutOverflowEmpty) {
  // This test documents the existing behavior so that we are aware when/if
  // it changes. It would also be reasonable for addLayoutOverflow to be
  // a no-op in this situation.
  m_overflow.addLayoutOverflow(LayoutRect(200, 200, 0, 0));
  EXPECT_EQ(LayoutRect(10, 10, 190, 190), m_overflow.layoutOverflowRect());
}

TEST_F(BoxOverflowModelTest, AddLayoutOverflowDoesNotAffectSelfVisualOverflow) {
  m_overflow.addLayoutOverflow(LayoutRect(300, 300, 300, 300));
  EXPECT_EQ(initialVisualOverflow(), m_overflow.selfVisualOverflowRect());
}

TEST_F(BoxOverflowModelTest,
       AddLayoutOverflowDoesNotAffectContentsVisualOverflow) {
  m_overflow.addLayoutOverflow(LayoutRect(300, 300, 300, 300));
  EXPECT_TRUE(m_overflow.contentsVisualOverflowRect().isEmpty());
}

TEST_F(BoxOverflowModelTest, AddSelfVisualOverflowOutsideExpandsRect) {
  m_overflow.addSelfVisualOverflow(LayoutRect(150, -50, 10, 10));
  EXPECT_EQ(LayoutRect(0, -50, 160, 150), m_overflow.selfVisualOverflowRect());
}

TEST_F(BoxOverflowModelTest, AddSelfVisualOverflowInsideDoesNotAffectRect) {
  m_overflow.addSelfVisualOverflow(LayoutRect(0, 10, 90, 90));
  EXPECT_EQ(initialVisualOverflow(), m_overflow.selfVisualOverflowRect());
}

TEST_F(BoxOverflowModelTest, AddSelfVisualOverflowEmpty) {
  BoxOverflowModel overflow(LayoutRect(), LayoutRect(0, 0, 600, 0));
  overflow.addSelfVisualOverflow(LayoutRect(100, -50, 100, 100));
  overflow.addSelfVisualOverflow(LayoutRect(300, 300, 0, 10000));
  EXPECT_EQ(LayoutRect(100, -50, 100, 100), overflow.selfVisualOverflowRect());
}

TEST_F(BoxOverflowModelTest, AddSelfVisualOverflowDoesNotAffectLayoutOverflow) {
  m_overflow.addSelfVisualOverflow(LayoutRect(300, 300, 300, 300));
  EXPECT_EQ(initialLayoutOverflow(), m_overflow.layoutOverflowRect());
}

TEST_F(BoxOverflowModelTest,
       AddSelfVisualOverflowDoesNotAffectContentsVisualOverflow) {
  m_overflow.addSelfVisualOverflow(LayoutRect(300, 300, 300, 300));
  EXPECT_TRUE(m_overflow.contentsVisualOverflowRect().isEmpty());
}

TEST_F(BoxOverflowModelTest, AddContentsVisualOverflowFirstCall) {
  m_overflow.addContentsVisualOverflow(LayoutRect(0, 0, 10, 10));
  EXPECT_EQ(LayoutRect(0, 0, 10, 10), m_overflow.contentsVisualOverflowRect());
}

TEST_F(BoxOverflowModelTest, AddContentsVisualOverflowUnitesRects) {
  m_overflow.addContentsVisualOverflow(LayoutRect(0, 0, 10, 10));
  m_overflow.addContentsVisualOverflow(LayoutRect(80, 80, 10, 10));
  EXPECT_EQ(LayoutRect(0, 0, 90, 90), m_overflow.contentsVisualOverflowRect());
}

TEST_F(BoxOverflowModelTest, AddContentsVisualOverflowRectWithinRect) {
  m_overflow.addContentsVisualOverflow(LayoutRect(0, 0, 10, 10));
  m_overflow.addContentsVisualOverflow(LayoutRect(2, 2, 5, 5));
  EXPECT_EQ(LayoutRect(0, 0, 10, 10), m_overflow.contentsVisualOverflowRect());
}

TEST_F(BoxOverflowModelTest, AddContentsVisualOverflowEmpty) {
  m_overflow.addContentsVisualOverflow(LayoutRect(0, 0, 10, 10));
  m_overflow.addContentsVisualOverflow(LayoutRect(20, 20, 0, 0));
  EXPECT_EQ(LayoutRect(0, 0, 10, 10), m_overflow.contentsVisualOverflowRect());
}

TEST_F(BoxOverflowModelTest, MoveAffectsLayoutOverflow) {
  m_overflow.move(LayoutUnit(500), LayoutUnit(100));
  EXPECT_EQ(LayoutRect(510, 110, 80, 80), m_overflow.layoutOverflowRect());
}

TEST_F(BoxOverflowModelTest, MoveAffectsSelfVisualOverflow) {
  m_overflow.move(LayoutUnit(500), LayoutUnit(100));
  EXPECT_EQ(LayoutRect(500, 100, 100, 100),
            m_overflow.selfVisualOverflowRect());
}

TEST_F(BoxOverflowModelTest, MoveAffectsContentsVisualOverflow) {
  m_overflow.addContentsVisualOverflow(LayoutRect(0, 0, 10, 10));
  m_overflow.move(LayoutUnit(500), LayoutUnit(100));
  EXPECT_EQ(LayoutRect(500, 100, 10, 10),
            m_overflow.contentsVisualOverflowRect());
}

}  // anonymous namespace
}  // namespace blink
