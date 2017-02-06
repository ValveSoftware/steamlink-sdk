// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/web_dialogs/web_dialog_delegate.h"

namespace ui {

std::string WebDialogDelegate::GetDialogName() const {
  return std::string();
}

void WebDialogDelegate::GetMinimumDialogSize(gfx::Size* size) const {
  GetDialogSize(size);
}

bool WebDialogDelegate::CanCloseDialog() const {
  return true;
}

bool WebDialogDelegate::CanResizeDialog() const {
  return true;
}

void WebDialogDelegate::OnDialogCloseFromWebUI(
    const std::string& json_retval) {
  OnDialogClosed(json_retval);
}

bool WebDialogDelegate::HandleContextMenu(
    const content::ContextMenuParams& params) {
  return false;
}

bool WebDialogDelegate::HandleOpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    content::WebContents** out_new_contents) {
  return false;
}

bool WebDialogDelegate::HandleAddNewContents(
    content::WebContents* source,
    content::WebContents* new_contents,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_rect,
    bool user_gesture) {
  return false;
}

bool WebDialogDelegate::HandleShouldCreateWebContents() {
  return true;
}

}  // namespace ui
