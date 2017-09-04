// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/message_bubble_base.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/message_center/message_center_style.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace {
// Delay laying out the MessageBubbleBase until all notifications have been
// added and icons have had a chance to load.
const int kUpdateDelayMs = 50;
const int kMessageBubbleBaseDefaultMaxHeight = 400;
}

namespace message_center {

const SkColor MessageBubbleBase::kBackgroundColor =
    SkColorSetRGB(0xfe, 0xfe, 0xfe);

MessageBubbleBase::MessageBubbleBase(MessageCenter* message_center,
                                     MessageCenterTray* tray)
    : message_center_(message_center),
      tray_(tray),
      bubble_view_(NULL),
      max_height_(kMessageBubbleBaseDefaultMaxHeight),
      weak_ptr_factory_(this) {
}

MessageBubbleBase::~MessageBubbleBase() {
  if (bubble_view_)
    bubble_view_->reset_delegate();
}

void MessageBubbleBase::BubbleViewDestroyed() {
  bubble_view_ = NULL;
  OnBubbleViewDestroyed();
}

void MessageBubbleBase::ScheduleUpdate() {
  weak_ptr_factory_.InvalidateWeakPtrs();  // Cancel any pending update.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&MessageBubbleBase::UpdateBubbleView,
                            weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kUpdateDelayMs));
}

bool MessageBubbleBase::IsVisible() const {
  return bubble_view() && bubble_view()->GetWidget()->IsVisible();
}

void MessageBubbleBase::SetMaxHeight(int height) {
  // Maximum height makes sense only for the new design.
  if (height == 0)
    height = kMessageBubbleBaseDefaultMaxHeight;
  if (height == max_height_)
    return;

  max_height_ = height;
  if (bubble_view_)
    bubble_view_->SetMaxHeight(max_height_);
}

views::TrayBubbleView::InitParams MessageBubbleBase::GetDefaultInitParams(
    views::TrayBubbleView::AnchorAlignment anchor_alignment) {
  views::TrayBubbleView::InitParams init_params(
      anchor_alignment,
      kNotificationWidth,
      kNotificationWidth);
  init_params.bg_color = kBackgroundDarkColor;
  return init_params;
}

}  // namespace message_center
