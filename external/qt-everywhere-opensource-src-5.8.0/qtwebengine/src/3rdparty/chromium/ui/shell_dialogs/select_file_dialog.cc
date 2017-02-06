// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/shell_dialogs/select_file_dialog.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace {

// Optional dialog factory. Leaked.
ui::SelectFileDialogFactory* dialog_factory_ = NULL;

}  // namespace

namespace ui {

SelectFileDialog::FileTypeInfo::FileTypeInfo()
    : include_all_files(false), allowed_paths(NATIVE_PATH) {}

SelectFileDialog::FileTypeInfo::FileTypeInfo(const FileTypeInfo& other) =
    default;

SelectFileDialog::FileTypeInfo::~FileTypeInfo() {}

void SelectFileDialog::Listener::FileSelectedWithExtraInfo(
    const ui::SelectedFileInfo& file,
    int index,
    void* params) {
  // Most of the dialogs need actual local path, so default to it.
  // If local path is empty, use file_path instead.
  FileSelected(file.local_path.empty() ? file.file_path : file.local_path,
               index,
               params);
}

void SelectFileDialog::Listener::MultiFilesSelectedWithExtraInfo(
    const std::vector<ui::SelectedFileInfo>& files,
    void* params) {
  std::vector<base::FilePath> file_paths;
  for (size_t i = 0; i < files.size(); ++i)
    file_paths.push_back(files[i].local_path.empty() ? files[i].file_path
                                                     : files[i].local_path);

  MultiFilesSelected(file_paths, params);
}

// static
void SelectFileDialog::SetFactory(ui::SelectFileDialogFactory* factory) {
  delete dialog_factory_;
  dialog_factory_ = factory;
}

// static
scoped_refptr<SelectFileDialog> SelectFileDialog::Create(
    Listener* listener,
    ui::SelectFilePolicy* policy) {
  if (dialog_factory_) {
    SelectFileDialog* dialog = dialog_factory_->Create(listener, policy);
    if (dialog)
      return dialog;
  }

  return CreateSelectFileDialog(listener, policy);
}

void SelectFileDialog::SelectFile(
    Type type,
    const base::string16& title,
    const base::FilePath& default_path,
    const FileTypeInfo* file_types,
    int file_type_index,
    const base::FilePath::StringType& default_extension,
    gfx::NativeWindow owning_window,
    void* params) {
  DCHECK(listener_);

  if (select_file_policy_.get() &&
      !select_file_policy_->CanOpenSelectFileDialog()) {
    select_file_policy_->SelectFileDenied();

    // Inform the listener that no file was selected.
    // Post a task rather than calling FileSelectionCanceled directly to ensure
    // that the listener is called asynchronously.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&SelectFileDialog::CancelFileSelection, this, params));
    return;
  }

  // Call the platform specific implementation of the file selection dialog.
  SelectFileImpl(type, title, default_path, file_types, file_type_index,
                 default_extension, owning_window, params);
}

bool SelectFileDialog::HasMultipleFileTypeChoices() {
  return HasMultipleFileTypeChoicesImpl();
}

SelectFileDialog::SelectFileDialog(Listener* listener,
                                   ui::SelectFilePolicy* policy)
    : listener_(listener),
      select_file_policy_(policy) {
  DCHECK(listener_);
}

SelectFileDialog::~SelectFileDialog() {}

void SelectFileDialog::CancelFileSelection(void* params) {
  if (listener_)
    listener_->FileSelectionCanceled(params);
}

}  // namespace ui
