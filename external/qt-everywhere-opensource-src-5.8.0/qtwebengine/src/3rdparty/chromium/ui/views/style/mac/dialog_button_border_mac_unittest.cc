// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/style/mac/dialog_button_border_mac.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/compositor/canvas_painter.h"
#include "ui/gfx/canvas.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/test/views_test_base.h"

namespace views {
namespace {

// LabelButton that can optionally provide a custom border.
class TestLabelButton : public LabelButton {
 public:
  explicit TestLabelButton(const char* text)
      : LabelButton(nullptr, base::ASCIIToUTF16(text)) {}

  void SimulateAddToWidget() { OnNativeThemeChanged(nullptr); }
  void set_provide_custom_border(bool value) { provide_custom_border_ = value; }

  // LabelButton:
  std::unique_ptr<LabelButtonBorder> CreateDefaultBorder() const override {
    if (!provide_custom_border_)
      return LabelButton::CreateDefaultBorder();

    return base::WrapUnique(new LabelButtonAssetBorder(style()));
  }

 private:
  bool provide_custom_border_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestLabelButton);
};

gfx::Size DialogButtonBorderMacSize() {
  const DialogButtonBorderMac template_border;
  return template_border.GetMinimumSize();
}

// A heuristic that tries to determine whether the border on |view| is a
// DialogButtonBorderMac by checking its minimum size.
bool BorderIsDialogButton(const View& view) {
  const Border* border = view.border();
  return border && DialogButtonBorderMacSize() == border->GetMinimumSize();
}

SkColor TestPaint(View* view) {
  EXPECT_TRUE(view->visible());
  EXPECT_FALSE(view->bounds().IsEmpty());
  const gfx::Point center = view->bounds().CenterPoint();
  gfx::Canvas canvas(view->bounds().size(), 1.0, false /* is_opaque */);
  SkCanvas* sk_canvas = canvas.sk_canvas();

  // Read a pixel - it should be blank.
  SkColor initial_pixel;
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  EXPECT_TRUE(sk_canvas->readPixels(&bitmap, center.x(), center.y()));
  initial_pixel = bitmap.getColor(0, 0);
  EXPECT_EQ(static_cast<SkColor>(SK_ColorTRANSPARENT), initial_pixel);

  view->Paint(ui::CanvasPainter(&canvas, 1.f).context());

  // Ensure save()/restore() calls are balanced.
  EXPECT_EQ(1, sk_canvas->getSaveCount());

  // Ensure "something" happened. This assumes the border is a
  // DialogButtonBorderMac, which always modifies the center pixel.
  EXPECT_TRUE(sk_canvas->readPixels(&bitmap, center.x(), center.y()));
  return bitmap.getColor(0, 0);
}

void TestPaintAllStates(CustomButton* button, bool verify) {
  for (int i = 0; i < Button::STATE_COUNT; ++i) {
    Button::ButtonState state = static_cast<Button::ButtonState>(i);
    SCOPED_TRACE(testing::Message() << "Button::ButtonState: " << state);
    button->SetState(state);
    SkColor color = TestPaint(button);
    if (verify)
      EXPECT_NE(static_cast<SkColor>(SK_ColorTRANSPARENT), color);
  }
}

}  // namespace

using DialogButtonBorderMacTest = ViewsTestBase;

// Verify that the DialogButtonBorderMac insets are consistent with the
// minimum size, and they're correctly carried across to the View's preferred
// size.
TEST_F(DialogButtonBorderMacTest, DrawMinimumSize) {
  TestLabelButton button("");
  button.SetStyle(Button::STYLE_BUTTON);
  button.SimulateAddToWidget();

  EXPECT_TRUE(BorderIsDialogButton(button));

  // The border minimum size should be at least the size of the insets.
  const gfx::Size border_min_size = DialogButtonBorderMacSize();
  const gfx::Insets insets = button.GetInsets();
  EXPECT_LE(insets.width(), border_min_size.width());
  EXPECT_LE(insets.height(), border_min_size.height());

  // The view preferred size should be at least as big as the border minimum.
  gfx::Size view_preferred_size = button.GetPreferredSize();
  EXPECT_LE(border_min_size.width(), view_preferred_size.width());
  EXPECT_LE(border_min_size.height(), view_preferred_size.height());

  // Note that Mac's PlatformStyle specifies a minimum button size, but it
  // shouldn't be larger than the size of the button's label plus border insets.
  // If it was, a Button::SetMinSize() call would be needed here to override it.

  button.SizeToPreferredSize();
  EXPECT_EQ(view_preferred_size.width(), button.width());
  EXPECT_EQ(view_preferred_size.height(), button.height());

  {
    SCOPED_TRACE("Preferred Size");
    TestPaintAllStates(&button, true);
  }

  // The View can ignore the border minimum size. To account for shadows, the
  // border will paint something as small as 4x4.
  {
    SCOPED_TRACE("Minimum Paint Size");
    button.SetSize(gfx::Size(4, 4));
    TestPaintAllStates(&button, true);
  }

  // Smaller than that, nothing gets painted, but the paint code should be sane.
  {
    SCOPED_TRACE("Size 1x1");
    button.SetSize(gfx::Size(1, 1));
    TestPaintAllStates(&button, false);
  }
}

// Test drawing with some text. The usual case.
TEST_F(DialogButtonBorderMacTest, DrawWithLabel) {
  TestLabelButton button("");
  button.SetStyle(Button::STYLE_BUTTON);
  button.SimulateAddToWidget();

  EXPECT_TRUE(BorderIsDialogButton(button));

  button.SizeToPreferredSize();
  const gfx::Size no_label_size = button.size();

  button.SetText(
      base::ASCIIToUTF16("Label Text That Exceeds the Minimum Button Size"));
  button.SizeToPreferredSize();

  // Long label, so the button width should be greater than the empty button.
  EXPECT_LT(no_label_size.width(), button.width());

  // The height shouldn't change.
  EXPECT_EQ(no_label_size.height(), button.height());

  TestPaintAllStates(&button, true);
}

// Test that the themed style is not used for STYLE_TEXTBUTTON (the default), or
// when a custom Border is set, or when a LabelButton subclass provides its own
// default border.
TEST_F(DialogButtonBorderMacTest, ChecksButtonStyle) {
  TestLabelButton button("");
  button.SimulateAddToWidget();

  // Default style is STYLE_TEXTBUTTON, which doesn't use the themed border.
  EXPECT_FALSE(BorderIsDialogButton(button));

  button.SetStyle(Button::STYLE_BUTTON);
  button.SimulateAddToWidget();
  EXPECT_TRUE(BorderIsDialogButton(button));

  button.set_provide_custom_border(true);
  button.SimulateAddToWidget();
  EXPECT_FALSE(BorderIsDialogButton(button));

  button.set_provide_custom_border(false);
  button.SimulateAddToWidget();
  EXPECT_TRUE(BorderIsDialogButton(button));

  // Any call to SetBorder() will immediately prevent themed buttons and adding
  // to a Widget (to pick up a NativeTheme) shouldn't restore them.
  button.SetBorder(Border::NullBorder());
  EXPECT_FALSE(BorderIsDialogButton(button));
  button.SimulateAddToWidget();
  EXPECT_FALSE(BorderIsDialogButton(button));
}

}  // namespace views
