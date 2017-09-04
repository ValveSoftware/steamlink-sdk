// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_REQUEST_FILE_SYSTEM_DIALOG_VIEW_H_
#define CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_REQUEST_FILE_SYSTEM_DIALOG_VIEW_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "extensions/common/extension.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
}  // namespace content

namespace file_manager {
class Volume;
}  // namespace file_manager

namespace views {
class View;
}  // namespace views

// Represents a dialog shown to a user for granting access to a file system.
class RequestFileSystemDialogView : public views::DialogDelegate {
 public:
  ~RequestFileSystemDialogView() override;

  // Shows the dialog and calls |callback| on completion.
  static void ShowDialog(
      content::WebContents* web_contents,
      const extensions::Extension& extension,
      base::WeakPtr<file_manager::Volume> volume,
      bool writable,
      const base::Callback<void(ui::DialogButton)>& callback);

  // views::DialogDelegate overrides:
  base::string16 GetAccessibleWindowTitle() const override;
  int GetDefaultDialogButton() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  ui::ModalType GetModalType() const override;
  views::View* GetContentsView() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;
  bool Cancel() override;
  bool Accept() override;

 private:
  RequestFileSystemDialogView(
      const extensions::Extension& extension,
      base::WeakPtr<file_manager::Volume> volume,
      bool writable,
      const base::Callback<void(ui::DialogButton)>& callback);

  const base::Callback<void(ui::DialogButton)> callback_;
  views::View* const contents_view_;

  DISALLOW_COPY_AND_ASSIGN(RequestFileSystemDialogView);
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_REQUEST_FILE_SYSTEM_DIALOG_VIEW_H_
