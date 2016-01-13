// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/blue_button.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/test/views_test_base.h"

namespace views {

namespace {

class TestBlueButton : public BlueButton {
 public:
  TestBlueButton() : BlueButton(NULL, base::ASCIIToUTF16("foo")) {}
  virtual ~TestBlueButton() {}

  using BlueButton::OnNativeThemeChanged;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestBlueButton);
};

}  // namespace

typedef ViewsTestBase BlueButtonTest;

TEST_F(BlueButtonTest, Border) {
  // Compared to a normal LabelButton...
  LabelButton button(NULL, base::ASCIIToUTF16("foo"));
  button.SetBoundsRect(gfx::Rect(gfx::Point(0, 0), button.GetPreferredSize()));
  gfx::Canvas button_canvas(button.bounds().size(), 1.0, true);
  button.border()->Paint(button, &button_canvas);

  // ... a special blue border should be used.
  TestBlueButton blue_button;
  blue_button.SetBoundsRect(gfx::Rect(gfx::Point(0, 0),
                            blue_button.GetPreferredSize()));
  gfx::Canvas canvas(blue_button.bounds().size(), 1.0, true);
  blue_button.border()->Paint(blue_button, &canvas);
  EXPECT_EQ(button.GetText(), blue_button.GetText());
  EXPECT_FALSE(gfx::BitmapsAreEqual(button_canvas.ExtractImageRep().sk_bitmap(),
                                    canvas.ExtractImageRep().sk_bitmap()));

  // Make sure it's still used after the native theme "changes".
  blue_button.OnNativeThemeChanged(NULL);
  gfx::Canvas canvas2(blue_button.bounds().size(), 1.0, true);
  blue_button.border()->Paint(blue_button, &canvas2);

  EXPECT_TRUE(gfx::BitmapsAreEqual(canvas.ExtractImageRep().sk_bitmap(),
                                   canvas2.ExtractImageRep().sk_bitmap()));
}

}  // namespace views
