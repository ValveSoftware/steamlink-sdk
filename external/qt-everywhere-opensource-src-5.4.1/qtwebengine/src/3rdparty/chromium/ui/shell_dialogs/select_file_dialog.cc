// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/shell_dialogs/select_file_dialog.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "build/build_config.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "ui/shell_dialogs/shell_dialogs_delegate.h"

#if defined(OS_WIN)
#include "ui/shell_dialogs/select_file_dialog_win.h"
#elif defined(OS_MACOSX)
#include "ui/shell_dialogs/select_file_dialog_mac.h"
#elif defined(OS_ANDROID)
#include "ui/shell_dialogs/select_file_dialog_android.h"
#elif defined(USE_AURA) && defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "ui/shell_dialogs/linux_shell_dialog.h"
#endif

namespace {

// Optional dialog factory. Leaked.
ui::SelectFileDialogFactory* dialog_factory_ = NULL;

// The global shell dialogs delegate.
ui::ShellDialogsDelegate* g_shell_dialogs_delegate_ = NULL;

}  // namespace

namespace ui {

SelectFileDialog::FileTypeInfo::FileTypeInfo()
    : include_all_files(false),
      support_drive(false) {}

SelectFileDialog::FileTypeInfo::~FileTypeInfo() {}

void SelectFileDialog::Listener::FileSelectedWithExtraInfo(
    const ui::SelectedFileInfo& file,
    int index,
    void* params) {
  // Most of the dialogs need actual local path, so default to it.
  FileSelected(file.local_path, index, params);
}

void SelectFileDialog::Listener::MultiFilesSelectedWithExtraInfo(
    const std::vector<ui::SelectedFileInfo>& files,
    void* params) {
  std::vector<base::FilePath> file_paths;
  for (size_t i = 0; i < files.size(); ++i)
    file_paths.push_back(files[i].local_path);

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

#if defined(USE_AURA) && defined(OS_LINUX) && !defined(OS_CHROMEOS)
  const ui::LinuxShellDialog* shell_dialogs = ui::LinuxShellDialog::instance();
  if (shell_dialogs)
    return shell_dialogs->CreateSelectFileDialog(listener, policy);
#endif

#if defined(OS_WIN)
  // TODO(ananta)
  // Fix this for Chrome ASH on Windows.
  return CreateWinSelectFileDialog(listener, policy);
#elif defined(OS_MACOSX) && !defined(USE_AURA)
  return CreateMacSelectFileDialog(listener, policy);
#elif defined(OS_ANDROID)
  return CreateAndroidSelectFileDialog(listener, policy);
#else
  NOTIMPLEMENTED();
  return NULL;
#endif
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
    base::MessageLoop::current()->PostTask(
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

// static
void SelectFileDialog::SetShellDialogsDelegate(ShellDialogsDelegate* delegate) {
  g_shell_dialogs_delegate_ = delegate;
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

ShellDialogsDelegate* SelectFileDialog::GetShellDialogsDelegate() {
  return g_shell_dialogs_delegate_;
}

}  // namespace ui
