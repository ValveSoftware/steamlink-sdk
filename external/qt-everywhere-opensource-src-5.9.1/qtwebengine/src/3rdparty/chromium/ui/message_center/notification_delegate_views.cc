// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/notification_delegate.h"

#include "ui/views/view.h"

namespace message_center {

std::unique_ptr<views::View> NotificationDelegate::CreateCustomContent() {
  return std::unique_ptr<views::View>();
}

}  // namespace message_center
