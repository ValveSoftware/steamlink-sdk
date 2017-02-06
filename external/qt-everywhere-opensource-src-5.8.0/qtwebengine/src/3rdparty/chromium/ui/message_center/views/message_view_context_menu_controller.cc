// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/message_view_context_menu_controller.h"

#include "ui/base/models/menu_model.h"
#include "ui/message_center/views/message_center_controller.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/widget.h"

namespace message_center {

MessageViewContextMenuController::MessageViewContextMenuController(
    MessageCenterController* controller)
    : controller_(controller) {
}

MessageViewContextMenuController::~MessageViewContextMenuController() {
}

void MessageViewContextMenuController::ShowContextMenuForView(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  // Assumes that the target view has to be MessageView.
  MessageView* message_view = static_cast<MessageView*>(source);
  std::unique_ptr<ui::MenuModel> menu_model(controller_->CreateMenuModel(
      message_view->notifier_id(), message_view->display_source()));

  if (!menu_model || menu_model->GetItemCount() == 0)
    return;

  views::MenuRunner menu_runner(menu_model.get(),
                                views::MenuRunner::HAS_MNEMONICS);

  ignore_result(menu_runner.RunMenuAt(source->GetWidget()->GetTopLevelWidget(),
                                      NULL,
                                      gfx::Rect(point, gfx::Size()),
                                      views::MENU_ANCHOR_TOPRIGHT,
                                      source_type));
}

}  // namespace message_center
