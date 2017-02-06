// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/blue_button.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/test/widget_test.h"

namespace views {

using BlueButtonTest = test::WidgetTest;

TEST_F(BlueButtonTest, Border) {
  // The buttons must be added to a Widget so that borders are correctly
  // applied once the NativeTheme is determined.
  Widget* widget = CreateTopLevelPlatformWidget();

  // Compared to a normal LabelButton...
  LabelButton* button = new LabelButton(nullptr, base::ASCIIToUTF16("foo"));
  EXPECT_EQ(Button::STYLE_TEXTBUTTON, button->style());
  // Focus painter by default.
  EXPECT_TRUE(button->focus_painter());

  // Switch to the same style as BlueButton for a more compelling comparison.
  button->SetStyle(Button::STYLE_BUTTON);
  EXPECT_EQ(Button::STYLE_BUTTON, button->style());
  EXPECT_FALSE(button->focus_painter());

  widget->GetContentsView()->AddChildView(button);
  button->SizeToPreferredSize();
  gfx::Canvas button_canvas(button->size(), 1.0, true);
  button->border()->Paint(*button, &button_canvas);

  // ... a special blue border should be used.
  BlueButton* blue_button = new BlueButton(nullptr, base::ASCIIToUTF16("foo"));
  EXPECT_EQ(Button::STYLE_BUTTON, blue_button->style());
  EXPECT_FALSE(blue_button->focus_painter());

  widget->GetContentsView()->AddChildView(blue_button);
  blue_button->SizeToPreferredSize();
#if defined(OS_MACOSX)
  // On Mac, themed STYLE_BUTTON buttons provide blue theming for dialog-default
  // buttons. This makes it unlikely that they will appear together with a
  // BlueButton on the same dialog. So the sizes don't really need to be
  // consistent. However, for the purposes of this test (e.g. to ensure we don't
  // accidentally make BlueButtons look like themed buttons on Mac), force the
  // sizes to match (ignoring the minimum size) so that the bitmaps can be
  // compared.
  EXPECT_NE(button->size(), blue_button->size());  // Verify this is needed.
  blue_button->SetSize(button->size());
#endif

  gfx::Canvas canvas(blue_button->size(), 1.0, true);
  blue_button->border()->Paint(*blue_button, &canvas);
  EXPECT_EQ(button->GetText(), blue_button->GetText());
  EXPECT_EQ(button->size(), blue_button->size());
  EXPECT_FALSE(gfx::BitmapsAreEqual(button_canvas.ExtractImageRep().sk_bitmap(),
                                    canvas.ExtractImageRep().sk_bitmap()));

  widget->CloseNow();
}

}  // namespace views
