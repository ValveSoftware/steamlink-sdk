// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/browser/debug_view.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace mash {
namespace browser {

DebugView::DebugView()
    : interstitial_container_(new views::View),
      interstitial_label_(
          new views::Label(base::ASCIIToUTF16("Interstitial: "))),
      interstitial_show_(
          new views::LabelButton(this, base::ASCIIToUTF16("Show"))),
      interstitial_hide_(
          new views::LabelButton(this, base::ASCIIToUTF16("Hide"))),
      interstitial_content_(new views::Textfield) {
  AddChildView(interstitial_container_);
  interstitial_container_->AddChildView(interstitial_label_);
  interstitial_container_->AddChildView(interstitial_show_);
  interstitial_container_->AddChildView(interstitial_hide_);
  interstitial_container_->AddChildView(interstitial_content_);
  views::BoxLayout* layout =
      new views::BoxLayout(views::BoxLayout::kHorizontal, 5, 5, 5);
  interstitial_container_->SetLayoutManager(layout);
  layout->set_main_axis_alignment(views::BoxLayout::MAIN_AXIS_ALIGNMENT_START);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_STRETCH);
  layout->SetDefaultFlex(0);
  layout->SetFlexForView(interstitial_content_, 1);

  set_background(views::Background::CreateStandardPanelBackground());
  SetLayoutManager(new views::FillLayout);
}

DebugView::~DebugView() {}

gfx::Size DebugView::GetPreferredSize() const {
  return interstitial_container_->GetPreferredSize();
}

void DebugView::OnPaint(gfx::Canvas* canvas) {
  gfx::Rect stroke_rect = GetLocalBounds();
  stroke_rect.set_height(1);
  canvas->FillRect(stroke_rect, SK_ColorGRAY);
}

void DebugView::ButtonPressed(views::Button* sender, const ui::Event& event) {
  DCHECK(view_);
  if (sender == interstitial_show_) {
    view_->ShowInterstitial(base::UTF16ToUTF8(interstitial_content_->text()));
  } else if (sender == interstitial_hide_) {
    view_->HideInterstitial();
  }
}

}  // namespace browser
}  // namespace mash
