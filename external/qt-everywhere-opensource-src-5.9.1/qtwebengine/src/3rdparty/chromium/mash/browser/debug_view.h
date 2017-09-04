// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_BROWSER_DEBUG_VIEW_H_
#define MASH_BROWSER_DEBUG_VIEW_H_

#include "services/navigation/public/cpp/view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class Label;
class LabelButton;
class Textfield;
}  // namespace views

namespace mash {
namespace browser {

// A panel that shows some controls for compelling the content area of the
// browser to do specific things.
class DebugView : public views::View,
                  public views::ButtonListener {
 public:
  DebugView();
  ~DebugView() override;
  DebugView(const DebugView&) = delete;
  void operator=(const DebugView&) = delete;

  void set_view(navigation::View* view) { view_ = view; }

  gfx::Size GetPreferredSize() const override;

 private:
  void OnPaint(gfx::Canvas* canvas) override;
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  navigation::View* view_ = nullptr;

  views::View* interstitial_container_;
  views::Label* interstitial_label_;
  views::LabelButton* interstitial_show_;
  views::LabelButton* interstitial_hide_;
  views::Textfield* interstitial_content_;
};

}  // namespace browser
}  // namespace mash

#endif  // MASH_BROWSER_DEBUG_VIEW_H_
