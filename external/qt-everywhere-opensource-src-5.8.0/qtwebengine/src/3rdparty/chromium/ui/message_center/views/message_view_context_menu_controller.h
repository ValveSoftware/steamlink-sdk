// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_MESSAGE_VIEW_CONTEXT_MENU_CONTROLLER_H_
#define UI_MESSAGE_CENTER_VIEWS_MESSAGE_VIEW_CONTEXT_MENU_CONTROLLER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/views/context_menu_controller.h"

namespace message_center {
class MessageCenterController;

class MessageViewContextMenuController : public views::ContextMenuController {
 public:
  explicit MessageViewContextMenuController(
      MessageCenterController* controller);
  ~MessageViewContextMenuController() override;

 private:
  // Overridden from views::ContextMenuController:
  void ShowContextMenuForView(views::View* source,
                              const gfx::Point& point,
                              ui::MenuSourceType source_type) override;

  MessageCenterController* controller_;

  DISALLOW_COPY_AND_ASSIGN(MessageViewContextMenuController);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_MESSAGE_VIEW_CONTEXT_MENU_CONTROLLER_H_
