// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/file_system/request_file_system_dialog_view.h"

#include <stddef.h>

#include <cstdlib>

#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font.h"
#include "ui/gfx/range/range.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_constants.h"

namespace {

// Maximum width of the dialog in pixels.
const int kDialogMaxWidth = 320;

}  // namespace

// static
void RequestFileSystemDialogView::ShowDialog(
    content::WebContents* web_contents,
    const extensions::Extension& extension,
    base::WeakPtr<file_manager::Volume> volume,
    bool writable,
    const base::Callback<void(ui::DialogButton)>& callback) {
  constrained_window::ShowWebModalDialogViews(
      new RequestFileSystemDialogView(extension, volume, writable, callback),
      web_contents);
}

RequestFileSystemDialogView::~RequestFileSystemDialogView() {
}

base::string16 RequestFileSystemDialogView::GetAccessibleWindowTitle() const {
  return l10n_util::GetStringUTF16(
      IDS_FILE_SYSTEM_REQUEST_FILE_SYSTEM_DIALOG_TITLE);
}

int RequestFileSystemDialogView::GetDefaultDialogButton() const {
  return ui::DIALOG_BUTTON_CANCEL;
}

base::string16 RequestFileSystemDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  switch (button) {
    case ui::DIALOG_BUTTON_OK:
      return l10n_util::GetStringUTF16(
          IDS_FILE_SYSTEM_REQUEST_FILE_SYSTEM_DIALOG_ALLOW_BUTTON);
    case ui::DIALOG_BUTTON_CANCEL:
      return l10n_util::GetStringUTF16(
          IDS_FILE_SYSTEM_REQUEST_FILE_SYSTEM_DIALOG_DENY_BUTTON);
    default:
      NOTREACHED();
  }
  return base::string16();
}

ui::ModalType RequestFileSystemDialogView::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

views::View* RequestFileSystemDialogView::GetContentsView() {
  return contents_view_;
}

views::Widget* RequestFileSystemDialogView::GetWidget() {
  return contents_view_->GetWidget();
}

const views::Widget* RequestFileSystemDialogView::GetWidget() const {
  return contents_view_->GetWidget();
}

bool RequestFileSystemDialogView::Cancel() {
  callback_.Run(ui::DIALOG_BUTTON_CANCEL);
  return true;
}

bool RequestFileSystemDialogView::Accept() {
  callback_.Run(ui::DIALOG_BUTTON_OK);
  return true;
}

RequestFileSystemDialogView::RequestFileSystemDialogView(
    const extensions::Extension& extension,
    base::WeakPtr<file_manager::Volume> volume,
    bool writable,
    const base::Callback<void(ui::DialogButton)>& callback)
    : callback_(callback), contents_view_(new views::View) {
  DCHECK(!callback_.is_null());

  // If the volume is gone, then cancel the dialog.
  if (!volume.get()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, ui::DIALOG_BUTTON_CANCEL));
    return;
  }

  const base::string16 app_name = base::UTF8ToUTF16(extension.name());
  // TODO(mtomasz): Improve the dialog contents, so it's easier for the user
  // to understand what device is being requested.
  const base::string16 volume_name =
      base::UTF8ToUTF16(!volume->volume_label().empty() ? volume->volume_label()
                                                        : volume->volume_id());
  std::vector<size_t> placeholder_offsets;
  const base::string16 message = l10n_util::GetStringFUTF16(
      writable ? IDS_FILE_SYSTEM_REQUEST_FILE_SYSTEM_DIALOG_WRITABLE_MESSAGE
               : IDS_FILE_SYSTEM_REQUEST_FILE_SYSTEM_DIALOG_MESSAGE,
      app_name, volume_name, &placeholder_offsets);

  views::StyledLabel* const label = new views::StyledLabel(message, nullptr);
  views::StyledLabel::RangeStyleInfo bold_style;
  bold_style.weight = gfx::Font::Weight::BOLD;

  DCHECK_EQ(2u, placeholder_offsets.size());
  label->AddStyleRange(gfx::Range(placeholder_offsets[0],
                                  placeholder_offsets[0] + app_name.length()),
                       bold_style);
  label->AddStyleRange(
      gfx::Range(placeholder_offsets[1],
                 placeholder_offsets[1] + volume_name.length()),
      bold_style);

  views::BoxLayout* const layout = new views::BoxLayout(
      views::BoxLayout::kHorizontal, views::kButtonHEdgeMarginNew,
      views::kPanelVertMargin, 0);
  contents_view_->SetLayoutManager(layout);

  label->SizeToFit(kDialogMaxWidth - 2 * views::kButtonHEdgeMarginNew);
  contents_view_->AddChildView(label);
}
