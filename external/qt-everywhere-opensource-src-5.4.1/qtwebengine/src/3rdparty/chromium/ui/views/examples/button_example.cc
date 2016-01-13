// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/examples/button_example.h"

#include "base/strings/utf_string_conversions.h"
#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

using base::ASCIIToUTF16;

namespace {
const char kLabelButton[] = "Label Button";
const char kTextButton[] = "Text Button";
const char kMultiLineText[] = "Multi-Line\nButton Text Is Here To Stay!\n123";
const char kLongText[] = "Start of Really Really Really Really Really Really "
                         "Really Really Really Really Really Really Really "
                         "Really Really Really Really Really Long Button Text";
}  // namespace

namespace views {
namespace examples {

ButtonExample::ButtonExample()
    : ExampleBase("Button"),
      text_button_(NULL),
      label_button_(NULL),
      image_button_(NULL),
      alignment_(TextButton::ALIGN_LEFT),
      use_native_theme_border_(false),
      icon_(NULL),
      count_(0) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  icon_ = rb.GetImageNamed(IDR_CLOSE_H).ToImageSkia();
}

ButtonExample::~ButtonExample() {
}

void ButtonExample::CreateExampleView(View* container) {
  container->set_background(Background::CreateSolidBackground(SK_ColorWHITE));
  container->SetLayoutManager(new BoxLayout(BoxLayout::kVertical, 10, 10, 10));

  text_button_ = new TextButton(this, ASCIIToUTF16(kTextButton));
  text_button_->SetFocusable(true);
  container->AddChildView(text_button_);

  label_button_ = new LabelButton(this, ASCIIToUTF16(kLabelButton));
  label_button_->SetFocusable(true);
  container->AddChildView(label_button_);

  LabelButton* disabled_button =
      new LabelButton(this, ASCIIToUTF16("Disabled Button"));
  disabled_button->SetStyle(Button::STYLE_BUTTON);
  disabled_button->SetState(Button::STATE_DISABLED);
  container->AddChildView(disabled_button);

  container->AddChildView(new BlueButton(this, ASCIIToUTF16("Blue Button")));

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  image_button_ = new ImageButton(this);
  image_button_->SetFocusable(true);
  image_button_->SetImage(ImageButton::STATE_NORMAL,
                          rb.GetImageNamed(IDR_CLOSE).ToImageSkia());
  image_button_->SetImage(ImageButton::STATE_HOVERED,
                          rb.GetImageNamed(IDR_CLOSE_H).ToImageSkia());
  image_button_->SetImage(ImageButton::STATE_PRESSED,
                          rb.GetImageNamed(IDR_CLOSE_P).ToImageSkia());
  container->AddChildView(image_button_);
}

void ButtonExample::TextButtonPressed(const ui::Event& event) {
  PrintStatus("Text Button Pressed! count: %d", ++count_);
  if (event.IsControlDown()) {
    if (event.IsShiftDown()) {
      if (event.IsAltDown()) {
        text_button_->SetMultiLine(!text_button_->multi_line());
        text_button_->SetText(ASCIIToUTF16(
            text_button_->multi_line() ? kMultiLineText : kTextButton));
      } else {
        switch (text_button_->icon_placement()) {
          case TextButton::ICON_ON_LEFT:
            text_button_->set_icon_placement(TextButton::ICON_ON_RIGHT);
            break;
          case TextButton::ICON_ON_RIGHT:
            text_button_->set_icon_placement(TextButton::ICON_ON_LEFT);
            break;
          case TextButton::ICON_CENTERED:
            // Do nothing.
            break;
        }
      }
    } else if (event.IsAltDown()) {
      if (text_button_->HasIcon())
        text_button_->SetIcon(gfx::ImageSkia());
      else
        text_button_->SetIcon(*icon_);
    } else {
      switch (alignment_) {
        case TextButton::ALIGN_LEFT:
          alignment_ = TextButton::ALIGN_CENTER;
          break;
        case TextButton::ALIGN_CENTER:
          alignment_ = TextButton::ALIGN_RIGHT;
          break;
        case TextButton::ALIGN_RIGHT:
          alignment_ = TextButton::ALIGN_LEFT;
          break;
      }
      text_button_->set_alignment(alignment_);
    }
  } else if (event.IsShiftDown()) {
    if (event.IsAltDown()) {
      text_button_->SetText(ASCIIToUTF16(
          text_button_->text().length() < 50 ? kLongText : kTextButton));
    } else {
      use_native_theme_border_ = !use_native_theme_border_;
      if (use_native_theme_border_) {
        text_button_->SetBorder(scoped_ptr<views::Border>(
            new TextButtonNativeThemeBorder(text_button_)));
      } else {
        text_button_->SetBorder(
            scoped_ptr<views::Border>(new TextButtonDefaultBorder()));
      }
    }
  } else if (event.IsAltDown()) {
    text_button_->SetIsDefault(!text_button_->is_default());
  } else {
    text_button_->ClearMaxTextSize();
  }
  example_view()->GetLayoutManager()->Layout(example_view());
}

void ButtonExample::LabelButtonPressed(const ui::Event& event) {
  PrintStatus("Label Button Pressed! count: %d", ++count_);
  if (event.IsControlDown()) {
    if (event.IsShiftDown()) {
      if (event.IsAltDown()) {
        label_button_->SetTextMultiLine(!label_button_->GetTextMultiLine());
        label_button_->SetText(ASCIIToUTF16(
            label_button_->GetTextMultiLine() ? kMultiLineText : kLabelButton));
      } else {
        label_button_->SetText(ASCIIToUTF16(
            label_button_->GetText().empty() ? kLongText :
                label_button_->GetText().length() > 50 ? kLabelButton : ""));
      }
    } else if (event.IsAltDown()) {
      label_button_->SetImage(Button::STATE_NORMAL,
          label_button_->GetImage(Button::STATE_NORMAL).isNull() ?
          *icon_ : gfx::ImageSkia());
    } else {
      label_button_->SetHorizontalAlignment(
          static_cast<gfx::HorizontalAlignment>(
              (label_button_->GetHorizontalAlignment() + 1) % 3));
    }
  } else if (event.IsShiftDown()) {
    if (event.IsAltDown()) {
      label_button_->SetFocusable(!label_button_->IsFocusable());
    } else {
      label_button_->SetStyle(static_cast<Button::ButtonStyle>(
          (label_button_->style() + 1) % Button::STYLE_COUNT));
    }
  } else if (event.IsAltDown()) {
    label_button_->SetIsDefault(!label_button_->is_default());
  } else {
    label_button_->set_min_size(gfx::Size());
  }
  example_view()->GetLayoutManager()->Layout(example_view());
}

void ButtonExample::ButtonPressed(Button* sender, const ui::Event& event) {
  if (sender == text_button_)
    TextButtonPressed(event);
  else if (sender == label_button_)
    LabelButtonPressed(event);
  else
    PrintStatus("Image Button Pressed! count: %d", ++count_);
}

}  // namespace examples
}  // namespace views
