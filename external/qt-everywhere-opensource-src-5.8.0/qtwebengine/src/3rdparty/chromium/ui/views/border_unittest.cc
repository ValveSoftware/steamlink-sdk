// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/border.h"

#include <algorithm>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/painter.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"

using namespace testing;

namespace {

class MockCanvas : public SkCanvas {
 public:
  struct DrawRectCall {
    DrawRectCall(const SkRect& rect, const SkPaint& paint)
        : rect(rect), paint(paint) {}

    bool operator<(const DrawRectCall& other) const {
      return std::tie(rect.fLeft, rect.fTop, rect.fRight, rect.fBottom) <
             std::tie(other.rect.fLeft, other.rect.fTop, other.rect.fRight,
                      other.rect.fBottom);
    }

    SkRect rect;
    SkPaint paint;
  };

  struct DrawRRectCall {
    DrawRRectCall(const SkRRect& rrect, const SkPaint& paint)
        : rrect(rrect), paint(paint) {}

    bool operator<(const DrawRRectCall& other) const {
      SkRect rect = rrect.rect();
      SkRect other_rect = other.rrect.rect();
      return std::tie(rect.fLeft, rect.fTop, rect.fRight, rect.fBottom) <
             std::tie(other_rect.fLeft, other_rect.fTop, other_rect.fRight,
                      other_rect.fBottom);
    }

    SkRRect rrect;
    SkPaint paint;
  };

  MockCanvas(int width, int height) : SkCanvas(width, height) {}

  // Return calls in sorted order.
  std::vector<DrawRectCall> draw_rect_calls() {
    return std::vector<DrawRectCall>(draw_rect_calls_.begin(),
                                     draw_rect_calls_.end());
  }

  // Return calls in sorted order.
  std::vector<DrawRRectCall> draw_rrect_calls() {
    return std::vector<DrawRRectCall>(draw_rrect_calls_.begin(),
                                      draw_rrect_calls_.end());
  }

  // SkCanvas overrides:
  void onDrawRect(const SkRect& rect, const SkPaint& paint) override {
    draw_rect_calls_.insert(DrawRectCall(rect, paint));
  }

  void onDrawRRect(const SkRRect& rrect, const SkPaint& paint) override {
    draw_rrect_calls_.insert(DrawRRectCall(rrect, paint));
  }

 private:
  // Stores all the calls for querying by the test, in sorted order.
  std::set<DrawRectCall> draw_rect_calls_;
  std::set<DrawRRectCall> draw_rrect_calls_;

  DISALLOW_COPY_AND_ASSIGN(MockCanvas);
};

// Simple Painter that will be used to test BorderPainter.
class MockPainter : public views::Painter {
 public:
  MockPainter() {}

  // Gets the canvas given to the last call to Paint().
  gfx::Canvas* given_canvas() const { return given_canvas_; }

  // Gets the size given to the last call to Paint().
  const gfx::Size& given_size() const { return given_size_; }

  // Painter overrides:
  gfx::Size GetMinimumSize() const override {
    // Just return some arbitrary size.
    return gfx::Size(60, 40);
  }

  void Paint(gfx::Canvas* canvas, const gfx::Size& size) override {
    // Just record the arguments.
    given_canvas_ = canvas;
    given_size_ = size;
  }

 private:
  gfx::Canvas* given_canvas_ = nullptr;
  gfx::Size given_size_;

  DISALLOW_COPY_AND_ASSIGN(MockPainter);
};

}  // namespace

namespace views {

class BorderTest : public ViewsTestBase {
 public:
  void SetUp() override {
    ViewsTestBase::SetUp();

    view_.reset(new views::View());
    view_->SetSize(gfx::Size(100, 50));
    // The canvas should be much bigger than the view.
    sk_canvas_.reset(new MockCanvas(1000, 500));
    canvas_.reset(new gfx::Canvas(sk_canvas_, 1.0f));
  }

  void TearDown() override {
    ViewsTestBase::TearDown();

    canvas_.reset();
    sk_canvas_.reset();
    view_.reset();
  }

 protected:
  std::unique_ptr<views::View> view_;
  sk_sp<MockCanvas> sk_canvas_;
  std::unique_ptr<gfx::Canvas> canvas_;
};

TEST_F(BorderTest, NullBorder) {
  std::unique_ptr<Border> border(Border::NullBorder());
  EXPECT_FALSE(border);
}

TEST_F(BorderTest, SolidBorder) {
  std::unique_ptr<Border> border(Border::CreateSolidBorder(3, SK_ColorBLUE));
  EXPECT_EQ(gfx::Size(6, 6), border->GetMinimumSize());
  EXPECT_EQ(gfx::Insets(3, 3, 3, 3), border->GetInsets());
  border->Paint(*view_, canvas_.get());

  std::vector<MockCanvas::DrawRectCall> draw_rect_calls =
      sk_canvas_->draw_rect_calls();
  ASSERT_EQ(4u, draw_rect_calls.size());
  EXPECT_EQ(SkRect::MakeLTRB(0, 0, 100, 3), draw_rect_calls[0].rect);
  EXPECT_EQ(SK_ColorBLUE, draw_rect_calls[0].paint.getColor());
  EXPECT_EQ(SkRect::MakeLTRB(0, 3, 3, 47), draw_rect_calls[1].rect);
  EXPECT_EQ(SK_ColorBLUE, draw_rect_calls[1].paint.getColor());
  EXPECT_EQ(SkRect::MakeLTRB(0, 47, 100, 50), draw_rect_calls[2].rect);
  EXPECT_EQ(SK_ColorBLUE, draw_rect_calls[2].paint.getColor());
  EXPECT_EQ(SkRect::MakeLTRB(97, 3, 100, 47), draw_rect_calls[3].rect);
  EXPECT_EQ(SK_ColorBLUE, draw_rect_calls[3].paint.getColor());

  EXPECT_TRUE(sk_canvas_->draw_rrect_calls().empty());
}

TEST_F(BorderTest, RoundedRectBorder) {
  std::unique_ptr<Border> border(
      Border::CreateRoundedRectBorder(3, 4, SK_ColorBLUE));
  EXPECT_EQ(gfx::Size(6, 6), border->GetMinimumSize());
  EXPECT_EQ(gfx::Insets(3, 3, 3, 3), border->GetInsets());
  border->Paint(*view_, canvas_.get());

  SkRRect expected_rrect;
  expected_rrect.setRectXY(SkRect::MakeLTRB(1.5, 1.5, 98.5, 48.5), 4, 4);
  EXPECT_TRUE(sk_canvas_->draw_rect_calls().empty());
  std::vector<MockCanvas::DrawRRectCall> draw_rrect_calls =
      sk_canvas_->draw_rrect_calls();
  ASSERT_EQ(1u, draw_rrect_calls.size());
  EXPECT_EQ(expected_rrect, draw_rrect_calls[0].rrect);
  EXPECT_EQ(3, draw_rrect_calls[0].paint.getStrokeWidth());
  EXPECT_EQ(SK_ColorBLUE, draw_rrect_calls[0].paint.getColor());
  EXPECT_EQ(SkPaint::kStroke_Style, draw_rrect_calls[0].paint.getStyle());
  EXPECT_TRUE(draw_rrect_calls[0].paint.isAntiAlias());
}

TEST_F(BorderTest, EmptyBorder) {
  const gfx::Insets kInsets(1, 2, 3, 4);

  std::unique_ptr<Border> border(Border::CreateEmptyBorder(
      kInsets.top(), kInsets.left(), kInsets.bottom(), kInsets.right()));
  // The EmptyBorder has no minimum size despite nonzero insets.
  EXPECT_EQ(gfx::Size(), border->GetMinimumSize());
  EXPECT_EQ(kInsets, border->GetInsets());
  // Should have no effect.
  border->Paint(*view_, canvas_.get());

  std::unique_ptr<Border> border2(Border::CreateEmptyBorder(kInsets));
  EXPECT_EQ(kInsets, border2->GetInsets());
}

TEST_F(BorderTest, SolidSidedBorder) {
  const gfx::Insets kInsets(1, 2, 3, 4);

  std::unique_ptr<Border> border(Border::CreateSolidSidedBorder(
      kInsets.top(), kInsets.left(), kInsets.bottom(), kInsets.right(),
      SK_ColorBLUE));
  EXPECT_EQ(gfx::Size(6, 4), border->GetMinimumSize());
  EXPECT_EQ(kInsets, border->GetInsets());
  border->Paint(*view_, canvas_.get());

  std::vector<MockCanvas::DrawRectCall> draw_rect_calls =
      sk_canvas_->draw_rect_calls();
  ASSERT_EQ(4u, draw_rect_calls.size());
  EXPECT_EQ(SkRect::MakeLTRB(0, 0, 100, 1), draw_rect_calls[0].rect);
  EXPECT_EQ(SK_ColorBLUE, draw_rect_calls[0].paint.getColor());
  EXPECT_EQ(SkRect::MakeLTRB(0, 1, 2, 47), draw_rect_calls[1].rect);
  EXPECT_EQ(SK_ColorBLUE, draw_rect_calls[1].paint.getColor());
  EXPECT_EQ(SkRect::MakeLTRB(0, 47, 100, 50), draw_rect_calls[2].rect);
  EXPECT_EQ(SK_ColorBLUE, draw_rect_calls[2].paint.getColor());
  EXPECT_EQ(SkRect::MakeLTRB(96, 1, 100, 47), draw_rect_calls[3].rect);
  EXPECT_EQ(SK_ColorBLUE, draw_rect_calls[3].paint.getColor());

  EXPECT_TRUE(sk_canvas_->draw_rrect_calls().empty());
}

TEST_F(BorderTest, BorderPainter) {
  const gfx::Insets kInsets(1, 2, 3, 4);

  std::unique_ptr<MockPainter> painter(new MockPainter());
  MockPainter* painter_ptr = painter.get();
  std::unique_ptr<Border> border(
      Border::CreateBorderPainter(std::move(painter), kInsets));
  EXPECT_EQ(gfx::Size(60, 40), border->GetMinimumSize());
  EXPECT_EQ(kInsets, border->GetInsets());

  border->Paint(*view_, canvas_.get());

  // Expect that the Painter was called with our canvas and the view's size.
  EXPECT_EQ(canvas_.get(), painter_ptr->given_canvas());
  EXPECT_EQ(view_->size(), painter_ptr->given_size());
}

}  // namespace views
