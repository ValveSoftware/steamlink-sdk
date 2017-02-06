// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/scrollbar/native_scroll_bar.h"

#include <algorithm>
#include <string>

#include "base/message_loop/message_loop.h"
#include "ui/events/event.h"
#include "ui/views/controls/scrollbar/native_scroll_bar_views.h"
#include "ui/views/controls/scrollbar/native_scroll_bar_wrapper.h"
#include "ui/views/widget/widget.h"

namespace views {

// static
const char NativeScrollBar::kViewClassName[] = "NativeScrollBar";

////////////////////////////////////////////////////////////////////////////////
// NativeScrollBar, public:
NativeScrollBar::NativeScrollBar(bool is_horizontal)
    : ScrollBar(is_horizontal),
      native_wrapper_(NULL) {
}

NativeScrollBar::~NativeScrollBar() {
}

// static
int NativeScrollBar::GetHorizontalScrollBarHeight(
    const ui::NativeTheme* theme) {
  return NativeScrollBarWrapper::GetHorizontalScrollBarHeight(theme);
}

// static
int NativeScrollBar::GetVerticalScrollBarWidth(
    const ui::NativeTheme* theme) {
  return NativeScrollBarWrapper::GetVerticalScrollBarWidth(theme);
}

////////////////////////////////////////////////////////////////////////////////
// NativeScrollBar, View overrides:
gfx::Size NativeScrollBar::GetPreferredSize() const {
  if (native_wrapper_)
    return native_wrapper_->GetView()->GetPreferredSize();
  return gfx::Size();
}

void NativeScrollBar::Layout() {
  if (native_wrapper_) {
    native_wrapper_->GetView()->SetBounds(0, 0, width(), height());
    native_wrapper_->GetView()->Layout();
  }
}

void NativeScrollBar::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add && !native_wrapper_ && GetWidget()) {
    native_wrapper_ = NativeScrollBarWrapper::CreateWrapper(this);
    AddChildView(native_wrapper_->GetView());
  }
}

const char* NativeScrollBar::GetClassName() const {
  return kViewClassName;
}

// Overridden from View for keyboard UI.
bool NativeScrollBar::OnKeyPressed(const ui::KeyEvent& event) {
  if (!native_wrapper_)
    return false;
  return native_wrapper_->GetView()->OnKeyPressed(event);
}

void NativeScrollBar::OnGestureEvent(ui::GestureEvent* event) {
  if (!native_wrapper_)
    return;
  native_wrapper_->GetView()->OnGestureEvent(event);
}

bool NativeScrollBar::OnMouseWheel(const ui::MouseWheelEvent& event) {
  if (!native_wrapper_)
    return false;
  return native_wrapper_->GetView()->OnMouseWheel(event);
}

////////////////////////////////////////////////////////////////////////////////
// NativeScrollBar, ScrollBar overrides:
void NativeScrollBar::Update(int viewport_size,
                             int content_size,
                             int current_pos) {
  ScrollBar::Update(viewport_size, content_size, current_pos);

  if (native_wrapper_)
    native_wrapper_->Update(viewport_size, content_size, current_pos);
}

int NativeScrollBar::GetLayoutSize() const {
  return IsHorizontal() ?
      GetHorizontalScrollBarHeight(GetNativeTheme()) :
      GetVerticalScrollBarWidth(GetNativeTheme());
}

int NativeScrollBar::GetPosition() const {
  if (!native_wrapper_)
    return 0;
  return native_wrapper_->GetPosition();
}

}  // namespace views

