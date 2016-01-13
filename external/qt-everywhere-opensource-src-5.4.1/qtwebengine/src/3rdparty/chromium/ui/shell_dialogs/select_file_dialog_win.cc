// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/shell_dialogs/select_file_dialog_win.h"

#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>

#include <algorithm>
#include <set>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/i18n/case_conversion.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread.h"
#include "base/win/metro.h"
#include "base/win/registry.h"
#include "base/win/scoped_comptr.h"
#include "base/win/shortcut.h"
#include "base/win/windows_version.h"
#include "grit/ui_strings.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/shell_dialogs/base_shell_dialog_win.h"
#include "ui/shell_dialogs/shell_dialogs_delegate.h"
#include "win8/viewer/metro_viewer_process_host.h"

namespace {

// Given |extension|, if it's not empty, then remove the leading dot.
std::wstring GetExtensionWithoutLeadingDot(const std::wstring& extension) {
  DCHECK(extension.empty() || extension[0] == L'.');
  return extension.empty() ? extension : extension.substr(1);
}

// Diverts to a metro-specific implementation as appropriate.
bool CallGetOpenFileName(OPENFILENAME* ofn) {
  HMODULE metro_module = base::win::GetMetroModule();
  if (metro_module != NULL) {
    typedef BOOL (*MetroGetOpenFileName)(OPENFILENAME*);
    MetroGetOpenFileName metro_get_open_file_name =
        reinterpret_cast<MetroGetOpenFileName>(
            ::GetProcAddress(metro_module, "MetroGetOpenFileName"));
    if (metro_get_open_file_name == NULL) {
      NOTREACHED();
      return false;
    }

    return metro_get_open_file_name(ofn) == TRUE;
  } else {
    return GetOpenFileName(ofn) == TRUE;
  }
}

// Diverts to a metro-specific implementation as appropriate.
bool CallGetSaveFileName(OPENFILENAME* ofn) {
  HMODULE metro_module = base::win::GetMetroModule();
  if (metro_module != NULL) {
    typedef BOOL (*MetroGetSaveFileName)(OPENFILENAME*);
    MetroGetSaveFileName metro_get_save_file_name =
        reinterpret_cast<MetroGetSaveFileName>(
            ::GetProcAddress(metro_module, "MetroGetSaveFileName"));
    if (metro_get_save_file_name == NULL) {
      NOTREACHED();
      return false;
    }

    return metro_get_save_file_name(ofn) == TRUE;
  } else {
    return GetSaveFileName(ofn) == TRUE;
  }
}

// Distinguish directories from regular files.
bool IsDirectory(const base::FilePath& path) {
  base::File::Info file_info;
  return base::GetFileInfo(path, &file_info) ?
      file_info.is_directory : path.EndsWithSeparator();
}

// Get the file type description from the registry. This will be "Text Document"
// for .txt files, "JPEG Image" for .jpg files, etc. If the registry doesn't
// have an entry for the file type, we return false, true if the description was
// found. 'file_ext' must be in form ".txt".
static bool GetRegistryDescriptionFromExtension(const std::wstring& file_ext,
                                                std::wstring* reg_description) {
  DCHECK(reg_description);
  base::win::RegKey reg_ext(HKEY_CLASSES_ROOT, file_ext.c_str(), KEY_READ);
  std::wstring reg_app;
  if (reg_ext.ReadValue(NULL, &reg_app) == ERROR_SUCCESS && !reg_app.empty()) {
    base::win::RegKey reg_link(HKEY_CLASSES_ROOT, reg_app.c_str(), KEY_READ);
    if (reg_link.ReadValue(NULL, reg_description) == ERROR_SUCCESS)
      return true;
  }
  return false;
}

// Set up a filter for a Save/Open dialog, which will consist of |file_ext| file
// extensions (internally separated by semicolons), |ext_desc| as the text
// descriptions of the |file_ext| types (optional), and (optionally) the default
// 'All Files' view. The purpose of the filter is to show only files of a
// particular type in a Windows Save/Open dialog box. The resulting filter is
// returned. The filters created here are:
//   1. only files that have 'file_ext' as their extension
//   2. all files (only added if 'include_all_files' is true)
// Example:
//   file_ext: { "*.txt", "*.htm;*.html" }
//   ext_desc: { "Text Document" }
//   returned: "Text Document\0*.txt\0HTML Document\0*.htm;*.html\0"
//             "All Files\0*.*\0\0" (in one big string)
// If a description is not provided for a file extension, it will be retrieved
// from the registry. If the file extension does not exist in the registry, it
// will be omitted from the filter, as it is likely a bogus extension.
std::wstring FormatFilterForExtensions(
    const std::vector<std::wstring>& file_ext,
    const std::vector<std::wstring>& ext_desc,
    bool include_all_files) {
  const std::wstring all_ext = L"*.*";
  const std::wstring all_desc =
      l10n_util::GetStringUTF16(IDS_APP_SAVEAS_ALL_FILES);

  DCHECK(file_ext.size() >= ext_desc.size());

  if (file_ext.empty())
    include_all_files = true;

  std::wstring result;

  for (size_t i = 0; i < file_ext.size(); ++i) {
    std::wstring ext = file_ext[i];
    std::wstring desc;
    if (i < ext_desc.size())
      desc = ext_desc[i];

    if (ext.empty()) {
      // Force something reasonable to appear in the dialog box if there is no
      // extension provided.
      include_all_files = true;
      continue;
    }

    if (desc.empty()) {
      DCHECK(ext.find(L'.') != std::wstring::npos);
      std::wstring first_extension = ext.substr(ext.find(L'.'));
      size_t first_separator_index = first_extension.find(L';');
      if (first_separator_index != std::wstring::npos)
        first_extension = first_extension.substr(0, first_separator_index);

      // Find the extension name without the preceeding '.' character.
      std::wstring ext_name = first_extension;
      size_t ext_index = ext_name.find_first_not_of(L'.');
      if (ext_index != std::wstring::npos)
        ext_name = ext_name.substr(ext_index);

      if (!GetRegistryDescriptionFromExtension(first_extension, &desc)) {
        // The extension doesn't exist in the registry. Create a description
        // based on the unknown extension type (i.e. if the extension is .qqq,
        // the we create a description "QQQ File (.qqq)").
        include_all_files = true;
        desc = l10n_util::GetStringFUTF16(
            IDS_APP_SAVEAS_EXTENSION_FORMAT,
            base::i18n::ToUpper(base::WideToUTF16(ext_name)),
            ext_name);
      }
      if (desc.empty())
        desc = L"*." + ext_name;
    }

    result.append(desc.c_str(), desc.size() + 1);  // Append NULL too.
    result.append(ext.c_str(), ext.size() + 1);
  }

  if (include_all_files) {
    result.append(all_desc.c_str(), all_desc.size() + 1);
    result.append(all_ext.c_str(), all_ext.size() + 1);
  }

  result.append(1, '\0');  // Double NULL required.
  return result;
}

// Enforce visible dialog box.
UINT_PTR CALLBACK SaveAsDialogHook(HWND dialog, UINT message,
                                   WPARAM wparam, LPARAM lparam) {
  static const UINT kPrivateMessage = 0x2F3F;
  switch (message) {
    case WM_INITDIALOG: {
      // Do nothing here. Just post a message to defer actual processing.
      PostMessage(dialog, kPrivateMessage, 0, 0);
      return TRUE;
    }
    case kPrivateMessage: {
      // The dialog box is the parent of the current handle.
      HWND real_dialog = GetParent(dialog);

      // Retrieve the final size.
      RECT dialog_rect;
      GetWindowRect(real_dialog, &dialog_rect);

      // Verify that the upper left corner is visible.
      POINT point = { dialog_rect.left, dialog_rect.top };
      HMONITOR monitor1 = MonitorFromPoint(point, MONITOR_DEFAULTTONULL);
      point.x = dialog_rect.right;
      point.y = dialog_rect.bottom;

      // Verify that the lower right corner is visible.
      HMONITOR monitor2 = MonitorFromPoint(point, MONITOR_DEFAULTTONULL);
      if (monitor1 && monitor2)
        return 0;

      // Some part of the dialog box is not visible, fix it by moving is to the
      // client rect position of the browser window.
      HWND parent_window = GetParent(real_dialog);
      if (!parent_window)
        return 0;
      WINDOWINFO parent_info;
      parent_info.cbSize = sizeof(WINDOWINFO);
      GetWindowInfo(parent_window, &parent_info);
      SetWindowPos(real_dialog, NULL,
                   parent_info.rcClient.left,
                   parent_info.rcClient.top,
                   0, 0,  // Size.
                   SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOSIZE |
                   SWP_NOZORDER);

      return 0;
    }
  }
  return 0;
}

// Prompt the user for location to save a file.
// Callers should provide the filter string, and also a filter index.
// The parameter |index| indicates the initial index of filter description
// and filter pattern for the dialog box. If |index| is zero or greater than
// the number of total filter types, the system uses the first filter in the
// |filter| buffer. |index| is used to specify the initial selected extension,
// and when done contains the extension the user chose. The parameter
// |final_name| returns the file name which contains the drive designator,
// path, file name, and extension of the user selected file name. |def_ext| is
// the default extension to give to the file if the user did not enter an
// extension. If |ignore_suggested_ext| is true, any file extension contained in
// |suggested_name| will not be used to generate the file name. This is useful
// in the case of saving web pages, where we know the extension type already and
// where |suggested_name| may contain a '.' character as a valid part of the
// name, thus confusing our extension detection code.
bool SaveFileAsWithFilter(HWND owner,
                          const std::wstring& suggested_name,
                          const std::wstring& filter,
                          const std::wstring& def_ext,
                          bool ignore_suggested_ext,
                          unsigned* index,
                          std::wstring* final_name) {
  DCHECK(final_name);
  // Having an empty filter makes for a bad user experience. We should always
  // specify a filter when saving.
  DCHECK(!filter.empty());
  const base::FilePath suggested_path(suggested_name);
  std::wstring file_part = suggested_path.BaseName().value();
  // If the suggested_name is a root directory, file_part will be '\', and the
  // call to GetSaveFileName below will fail.
  if (file_part.size() == 1 && file_part[0] == L'\\')
    file_part.clear();

  // The size of the in/out buffer in number of characters we pass to win32
  // GetSaveFileName.  From MSDN "The buffer must be large enough to store the
  // path and file name string or strings, including the terminating NULL
  // character.  ... The buffer should be at least 256 characters long.".
  // _IsValidPathComDlg does a copy expecting at most MAX_PATH, otherwise will
  // result in an error of FNERR_INVALIDFILENAME.  So we should only pass the
  // API a buffer of at most MAX_PATH.
  wchar_t file_name[MAX_PATH];
  base::wcslcpy(file_name, file_part.c_str(), arraysize(file_name));

  OPENFILENAME save_as;
  // We must do this otherwise the ofn's FlagsEx may be initialized to random
  // junk in release builds which can cause the Places Bar not to show up!
  ZeroMemory(&save_as, sizeof(save_as));
  save_as.lStructSize = sizeof(OPENFILENAME);
  save_as.hwndOwner = owner;
  save_as.hInstance = NULL;

  save_as.lpstrFilter = filter.empty() ? NULL : filter.c_str();

  save_as.lpstrCustomFilter = NULL;
  save_as.nMaxCustFilter = 0;
  save_as.nFilterIndex = *index;
  save_as.lpstrFile = file_name;
  save_as.nMaxFile = arraysize(file_name);
  save_as.lpstrFileTitle = NULL;
  save_as.nMaxFileTitle = 0;

  // Set up the initial directory for the dialog.
  std::wstring directory;
  if (!suggested_name.empty()) {
    if (IsDirectory(suggested_path)) {
      directory = suggested_path.value();
      file_part.clear();
    } else {
      directory = suggested_path.DirName().value();
    }
  }

  save_as.lpstrInitialDir = directory.c_str();
  save_as.lpstrTitle = NULL;
  save_as.Flags = OFN_OVERWRITEPROMPT | OFN_EXPLORER | OFN_ENABLESIZING |
                  OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST;
  save_as.lpstrDefExt = &def_ext[0];
  save_as.lCustData = NULL;

  if (base::win::GetVersion() < base::win::VERSION_VISTA) {
    // The save as on Windows XP remembers its last position,
    // and if the screen resolution changed, it will be off screen.
    save_as.Flags |= OFN_ENABLEHOOK;
    save_as.lpfnHook = &SaveAsDialogHook;
  }

  // Must be NULL or 0.
  save_as.pvReserved = NULL;
  save_as.dwReserved = 0;

  if (!CallGetSaveFileName(&save_as)) {
    // Zero means the dialog was closed, otherwise we had an error.
    DWORD error_code = CommDlgExtendedError();
    if (error_code != 0) {
      NOTREACHED() << "GetSaveFileName failed with code: " << error_code;
    }
    return false;
  }

  // Return the user's choice.
  final_name->assign(save_as.lpstrFile);
  *index = save_as.nFilterIndex;

  // Figure out what filter got selected from the vector with embedded nulls.
  // NOTE: The filter contains a string with embedded nulls, such as:
  // JPG Image\0*.jpg\0All files\0*.*\0\0
  // The filter index is 1-based index for which pair got selected. So, using
  // the example above, if the first index was selected we need to skip 1
  // instance of null to get to "*.jpg".
  std::vector<std::wstring> filters;
  if (!filter.empty() && save_as.nFilterIndex > 0)
    base::SplitString(filter, '\0', &filters);
  std::wstring filter_selected;
  if (!filters.empty())
    filter_selected = filters[(2 * (save_as.nFilterIndex - 1)) + 1];

  // Get the extension that was suggested to the user (when the Save As dialog
  // was opened). For saving web pages, we skip this step since there may be
  // 'extension characters' in the title of the web page.
  std::wstring suggested_ext;
  if (!ignore_suggested_ext)
    suggested_ext = GetExtensionWithoutLeadingDot(suggested_path.Extension());

  // If we can't get the extension from the suggested_name, we use the default
  // extension passed in. This is to cover cases like when saving a web page,
  // where we get passed in a name without an extension and a default extension
  // along with it.
  if (suggested_ext.empty())
    suggested_ext = def_ext;

  *final_name =
      ui::AppendExtensionIfNeeded(*final_name, filter_selected, suggested_ext);
  return true;
}

// Prompt the user for location to save a file. 'suggested_name' is a full path
// that gives the dialog box a hint as to how to initialize itself.
// For example, a 'suggested_name' of:
//   "C:\Documents and Settings\jojo\My Documents\picture.png"
// will start the dialog in the "C:\Documents and Settings\jojo\My Documents\"
// directory, and filter for .png file types.
// 'owner' is the window to which the dialog box is modal, NULL for a modeless
// dialog box.
// On success,  returns true and 'final_name' contains the full path of the file
// that the user chose. On error, returns false, and 'final_name' is not
// modified.
bool SaveFileAs(HWND owner,
                const std::wstring& suggested_name,
                std::wstring* final_name) {
  std::wstring file_ext =
      base::FilePath(suggested_name).Extension().insert(0, L"*");
  std::wstring filter = FormatFilterForExtensions(
      std::vector<std::wstring>(1, file_ext),
      std::vector<std::wstring>(),
      true);
  unsigned index = 1;
  return SaveFileAsWithFilter(owner,
                              suggested_name,
                              filter,
                              L"",
                              false,
                              &index,
                              final_name);
}

// Implementation of SelectFileDialog that shows a Windows common dialog for
// choosing a file or folder.
class SelectFileDialogImpl : public ui::SelectFileDialog,
                             public ui::BaseShellDialogImpl {
 public:
  explicit SelectFileDialogImpl(Listener* listener,
                                ui::SelectFilePolicy* policy);

  // BaseShellDialog implementation:
  virtual bool IsRunning(gfx::NativeWindow owning_window) const OVERRIDE;
  virtual void ListenerDestroyed() OVERRIDE;

 protected:
  // SelectFileDialog implementation:
  virtual void SelectFileImpl(
      Type type,
      const base::string16& title,
      const base::FilePath& default_path,
      const FileTypeInfo* file_types,
      int file_type_index,
      const base::FilePath::StringType& default_extension,
      gfx::NativeWindow owning_window,
      void* params) OVERRIDE;

 private:
  virtual ~SelectFileDialogImpl();

  // A struct for holding all the state necessary for displaying a Save dialog.
  struct ExecuteSelectParams {
    ExecuteSelectParams(Type type,
                        const std::wstring& title,
                        const base::FilePath& default_path,
                        const FileTypeInfo* file_types,
                        int file_type_index,
                        const std::wstring& default_extension,
                        RunState run_state,
                        HWND owner,
                        void* params)
        : type(type),
          title(title),
          default_path(default_path),
          file_type_index(file_type_index),
          default_extension(default_extension),
          run_state(run_state),
          ui_proxy(base::MessageLoopForUI::current()->message_loop_proxy()),
          owner(owner),
          params(params) {
      if (file_types)
        this->file_types = *file_types;
    }
    SelectFileDialog::Type type;
    std::wstring title;
    base::FilePath default_path;
    FileTypeInfo file_types;
    int file_type_index;
    std::wstring default_extension;
    RunState run_state;
    scoped_refptr<base::MessageLoopProxy> ui_proxy;
    HWND owner;
    void* params;
  };

  // Shows the file selection dialog modal to |owner| and calls the result
  // back on the ui thread. Run on the dialog thread.
  void ExecuteSelectFile(const ExecuteSelectParams& params);

  // Notifies the listener that a folder was chosen. Run on the ui thread.
  void FileSelected(const base::FilePath& path, int index,
                    void* params, RunState run_state);

  // Notifies listener that multiple files were chosen. Run on the ui thread.
  void MultiFilesSelected(const std::vector<base::FilePath>& paths,
                         void* params,
                         RunState run_state);

  // Notifies the listener that no file was chosen (the action was canceled).
  // Run on the ui thread.
  void FileNotSelected(void* params, RunState run_state);

  // Runs a Folder selection dialog box, passes back the selected folder in
  // |path| and returns true if the user clicks OK. If the user cancels the
  // dialog box the value in |path| is not modified and returns false. |title|
  // is the user-supplied title text to show for the dialog box. Run on the
  // dialog thread.
  bool RunSelectFolderDialog(const std::wstring& title,
                             HWND owner,
                             base::FilePath* path);

  // Runs an Open file dialog box, with similar semantics for input paramaters
  // as RunSelectFolderDialog.
  bool RunOpenFileDialog(const std::wstring& title,
                         const std::wstring& filters,
                         HWND owner,
                         base::FilePath* path);

  // Runs an Open file dialog box that supports multi-select, with similar
  // semantics for input paramaters as RunOpenFileDialog.
  bool RunOpenMultiFileDialog(const std::wstring& title,
                              const std::wstring& filter,
                              HWND owner,
                              std::vector<base::FilePath>* paths);

  // The callback function for when the select folder dialog is opened.
  static int CALLBACK BrowseCallbackProc(HWND window, UINT message,
                                         LPARAM parameter,
                                         LPARAM data);

  virtual bool HasMultipleFileTypeChoicesImpl() OVERRIDE;

  // Returns the filter to be used while displaying the open/save file dialog.
  // This is computed from the extensions for the file types being opened.
  // |file_types| can be NULL in which case the returned filter will be empty.
  base::string16 GetFilterForFileTypes(const FileTypeInfo* file_types);

  bool has_multiple_file_type_choices_;

  DISALLOW_COPY_AND_ASSIGN(SelectFileDialogImpl);
};

SelectFileDialogImpl::SelectFileDialogImpl(Listener* listener,
                                           ui::SelectFilePolicy* policy)
    : SelectFileDialog(listener, policy),
      BaseShellDialogImpl(),
      has_multiple_file_type_choices_(false) {
}

SelectFileDialogImpl::~SelectFileDialogImpl() {
}

void SelectFileDialogImpl::SelectFileImpl(
    Type type,
    const base::string16& title,
    const base::FilePath& default_path,
    const FileTypeInfo* file_types,
    int file_type_index,
    const base::FilePath::StringType& default_extension,
    gfx::NativeWindow owning_window,
    void* params) {
  has_multiple_file_type_choices_ =
      file_types ? file_types->extensions.size() > 1 : true;
  // If the owning_window passed in is in metro then we need to forward the
  // file open/save operations to metro.
  if (GetShellDialogsDelegate() &&
      GetShellDialogsDelegate()->IsWindowInMetro(owning_window)) {
    if (type == SELECT_SAVEAS_FILE) {
      win8::MetroViewerProcessHost::HandleSaveFile(
          title,
          default_path,
          GetFilterForFileTypes(file_types),
          file_type_index,
          default_extension,
          base::Bind(&ui::SelectFileDialog::Listener::FileSelected,
                     base::Unretained(listener_)),
          base::Bind(&ui::SelectFileDialog::Listener::FileSelectionCanceled,
                     base::Unretained(listener_)));
      return;
    } else if (type == SELECT_OPEN_FILE) {
      win8::MetroViewerProcessHost::HandleOpenFile(
          title,
          default_path,
          GetFilterForFileTypes(file_types),
          base::Bind(&ui::SelectFileDialog::Listener::FileSelected,
                     base::Unretained(listener_)),
          base::Bind(&ui::SelectFileDialog::Listener::FileSelectionCanceled,
                     base::Unretained(listener_)));
      return;
    } else if (type == SELECT_OPEN_MULTI_FILE) {
      win8::MetroViewerProcessHost::HandleOpenMultipleFiles(
          title,
          default_path,
          GetFilterForFileTypes(file_types),
          base::Bind(&ui::SelectFileDialog::Listener::MultiFilesSelected,
                     base::Unretained(listener_)),
          base::Bind(&ui::SelectFileDialog::Listener::FileSelectionCanceled,
                     base::Unretained(listener_)));
      return;
    } else if (type == SELECT_FOLDER || type == SELECT_UPLOAD_FOLDER) {
      base::string16 title_string = title;
      if (type == SELECT_UPLOAD_FOLDER && title_string.empty()) {
        // If it's for uploading don't use default dialog title to
        // make sure we clearly tell it's for uploading.
        title_string = l10n_util::GetStringUTF16(
            IDS_SELECT_UPLOAD_FOLDER_DIALOG_TITLE);
      }
      win8::MetroViewerProcessHost::HandleSelectFolder(
          title_string,
          base::Bind(&ui::SelectFileDialog::Listener::FileSelected,
                     base::Unretained(listener_)),
          base::Bind(&ui::SelectFileDialog::Listener::FileSelectionCanceled,
                     base::Unretained(listener_)));
      return;
    }
  }
  HWND owner = owning_window && owning_window->GetRootWindow()
      ? owning_window->GetHost()->GetAcceleratedWidget() : NULL;

  ExecuteSelectParams execute_params(type, title,
                                     default_path, file_types, file_type_index,
                                     default_extension, BeginRun(owner),
                                     owner, params);
  execute_params.run_state.dialog_thread->message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&SelectFileDialogImpl::ExecuteSelectFile, this,
                 execute_params));
}

bool SelectFileDialogImpl::HasMultipleFileTypeChoicesImpl() {
  return has_multiple_file_type_choices_;
}

bool SelectFileDialogImpl::IsRunning(gfx::NativeWindow owning_window) const {
  if (!owning_window->GetRootWindow())
    return false;
  HWND owner = owning_window->GetHost()->GetAcceleratedWidget();
  return listener_ && IsRunningDialogForOwner(owner);
}

void SelectFileDialogImpl::ListenerDestroyed() {
  // Our associated listener has gone away, so we shouldn't call back to it if
  // our worker thread returns after the listener is dead.
  listener_ = NULL;
}

void SelectFileDialogImpl::ExecuteSelectFile(
    const ExecuteSelectParams& params) {
  base::string16 filter = GetFilterForFileTypes(&params.file_types);

  base::FilePath path = params.default_path;
  bool success = false;
  unsigned filter_index = params.file_type_index;
  if (params.type == SELECT_FOLDER || params.type == SELECT_UPLOAD_FOLDER) {
    std::wstring title = params.title;
    if (title.empty() && params.type == SELECT_UPLOAD_FOLDER) {
      // If it's for uploading don't use default dialog title to
      // make sure we clearly tell it's for uploading.
      title = l10n_util::GetStringUTF16(IDS_SELECT_UPLOAD_FOLDER_DIALOG_TITLE);
    }
    success = RunSelectFolderDialog(title,
                                    params.run_state.owner,
                                    &path);
  } else if (params.type == SELECT_SAVEAS_FILE) {
    std::wstring path_as_wstring = path.value();
    success = SaveFileAsWithFilter(params.run_state.owner,
        params.default_path.value(), filter,
        params.default_extension, false, &filter_index, &path_as_wstring);
    if (success)
      path = base::FilePath(path_as_wstring);
    DisableOwner(params.run_state.owner);
  } else if (params.type == SELECT_OPEN_FILE) {
    success = RunOpenFileDialog(params.title, filter,
                                params.run_state.owner, &path);
  } else if (params.type == SELECT_OPEN_MULTI_FILE) {
    std::vector<base::FilePath> paths;
    if (RunOpenMultiFileDialog(params.title, filter,
                               params.run_state.owner, &paths)) {
      params.ui_proxy->PostTask(
          FROM_HERE,
          base::Bind(&SelectFileDialogImpl::MultiFilesSelected, this, paths,
                     params.params, params.run_state));
      return;
    }
  }

  if (success) {
      params.ui_proxy->PostTask(
        FROM_HERE,
        base::Bind(&SelectFileDialogImpl::FileSelected, this, path,
                   filter_index, params.params, params.run_state));
  } else {
      params.ui_proxy->PostTask(
        FROM_HERE,
        base::Bind(&SelectFileDialogImpl::FileNotSelected, this, params.params,
                   params.run_state));
  }
}

void SelectFileDialogImpl::FileSelected(const base::FilePath& selected_folder,
                                        int index,
                                        void* params,
                                        RunState run_state) {
  if (listener_)
    listener_->FileSelected(selected_folder, index, params);
  EndRun(run_state);
}

void SelectFileDialogImpl::MultiFilesSelected(
    const std::vector<base::FilePath>& selected_files,
    void* params,
    RunState run_state) {
  if (listener_)
    listener_->MultiFilesSelected(selected_files, params);
  EndRun(run_state);
}

void SelectFileDialogImpl::FileNotSelected(void* params, RunState run_state) {
  if (listener_)
    listener_->FileSelectionCanceled(params);
  EndRun(run_state);
}

int CALLBACK SelectFileDialogImpl::BrowseCallbackProc(HWND window,
                                                      UINT message,
                                                      LPARAM parameter,
                                                      LPARAM data) {
  if (message == BFFM_INITIALIZED) {
    // WParam is TRUE since passing a path.
    // data lParam member of the BROWSEINFO structure.
    SendMessage(window, BFFM_SETSELECTION, TRUE, (LPARAM)data);
  }
  return 0;
}

bool SelectFileDialogImpl::RunSelectFolderDialog(const std::wstring& title,
                                                 HWND owner,
                                                 base::FilePath* path) {
  DCHECK(path);

  wchar_t dir_buffer[MAX_PATH + 1];

  bool result = false;
  BROWSEINFO browse_info = {0};
  browse_info.hwndOwner = owner;
  browse_info.lpszTitle = title.c_str();
  browse_info.pszDisplayName = dir_buffer;
  browse_info.ulFlags = BIF_USENEWUI | BIF_RETURNONLYFSDIRS;

  if (path->value().length()) {
    // Highlight the current value.
    browse_info.lParam = (LPARAM)path->value().c_str();
    browse_info.lpfn = &BrowseCallbackProc;
  }

  LPITEMIDLIST list = SHBrowseForFolder(&browse_info);
  DisableOwner(owner);
  if (list) {
    STRRET out_dir_buffer;
    ZeroMemory(&out_dir_buffer, sizeof(out_dir_buffer));
    out_dir_buffer.uType = STRRET_WSTR;
    base::win::ScopedComPtr<IShellFolder> shell_folder;
    if (SHGetDesktopFolder(shell_folder.Receive()) == NOERROR) {
      HRESULT hr = shell_folder->GetDisplayNameOf(list, SHGDN_FORPARSING,
                                                  &out_dir_buffer);
      if (SUCCEEDED(hr) && out_dir_buffer.uType == STRRET_WSTR) {
        *path = base::FilePath(out_dir_buffer.pOleStr);
        CoTaskMemFree(out_dir_buffer.pOleStr);
        result = true;
      } else {
        // Use old way if we don't get what we want.
        wchar_t old_out_dir_buffer[MAX_PATH + 1];
        if (SHGetPathFromIDList(list, old_out_dir_buffer)) {
          *path = base::FilePath(old_out_dir_buffer);
          result = true;
        }
      }

      // According to MSDN, win2000 will not resolve shortcuts, so we do it
      // ourself.
      base::win::ResolveShortcut(*path, path, NULL);
    }
    CoTaskMemFree(list);
  }
  return result;
}

bool SelectFileDialogImpl::RunOpenFileDialog(
    const std::wstring& title,
    const std::wstring& filter,
    HWND owner,
    base::FilePath* path) {
  OPENFILENAME ofn;
  // We must do this otherwise the ofn's FlagsEx may be initialized to random
  // junk in release builds which can cause the Places Bar not to show up!
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = owner;

  wchar_t filename[MAX_PATH];
  // According to http://support.microsoft.com/?scid=kb;en-us;222003&x=8&y=12,
  // The lpstrFile Buffer MUST be NULL Terminated.
  filename[0] = 0;
  // Define the dir in here to keep the string buffer pointer pointed to
  // ofn.lpstrInitialDir available during the period of running the
  // GetOpenFileName.
  base::FilePath dir;
  // Use lpstrInitialDir to specify the initial directory
  if (!path->empty()) {
    if (IsDirectory(*path)) {
      ofn.lpstrInitialDir = path->value().c_str();
    } else {
      dir = path->DirName();
      ofn.lpstrInitialDir = dir.value().c_str();
      // Only pure filename can be put in lpstrFile field.
      base::wcslcpy(filename, path->BaseName().value().c_str(),
                    arraysize(filename));
    }
  }

  ofn.lpstrFile = filename;
  ofn.nMaxFile = MAX_PATH;

  // We use OFN_NOCHANGEDIR so that the user can rename or delete the directory
  // without having to close Chrome first.
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

  if (!filter.empty())
    ofn.lpstrFilter = filter.c_str();
  bool success = CallGetOpenFileName(&ofn);
  DisableOwner(owner);
  if (success)
    *path = base::FilePath(filename);
  return success;
}

bool SelectFileDialogImpl::RunOpenMultiFileDialog(
    const std::wstring& title,
    const std::wstring& filter,
    HWND owner,
    std::vector<base::FilePath>* paths) {
  OPENFILENAME ofn;
  // We must do this otherwise the ofn's FlagsEx may be initialized to random
  // junk in release builds which can cause the Places Bar not to show up!
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = owner;

  scoped_ptr<wchar_t[]> filename(new wchar_t[UNICODE_STRING_MAX_CHARS]);
  filename[0] = 0;

  ofn.lpstrFile = filename.get();
  ofn.nMaxFile = UNICODE_STRING_MAX_CHARS;
  // We use OFN_NOCHANGEDIR so that the user can rename or delete the directory
  // without having to close Chrome first.
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER
               | OFN_HIDEREADONLY | OFN_ALLOWMULTISELECT | OFN_NOCHANGEDIR;

  if (!filter.empty()) {
    ofn.lpstrFilter = filter.c_str();
  }

  bool success = CallGetOpenFileName(&ofn);
  DisableOwner(owner);
  if (success) {
    std::vector<base::FilePath> files;
    const wchar_t* selection = ofn.lpstrFile;
    while (*selection) {  // Empty string indicates end of list.
      files.push_back(base::FilePath(selection));
      // Skip over filename and null-terminator.
      selection += files.back().value().length() + 1;
    }
    if (files.empty()) {
      success = false;
    } else if (files.size() == 1) {
      // When there is one file, it contains the path and filename.
      paths->swap(files);
    } else {
      // Otherwise, the first string is the path, and the remainder are
      // filenames.
      std::vector<base::FilePath>::iterator path = files.begin();
      for (std::vector<base::FilePath>::iterator file = path + 1;
           file != files.end(); ++file) {
        paths->push_back(path->Append(*file));
      }
    }
  }
  return success;
}

base::string16 SelectFileDialogImpl::GetFilterForFileTypes(
    const FileTypeInfo* file_types) {
  if (!file_types)
    return base::string16();

  std::vector<base::string16> exts;
  for (size_t i = 0; i < file_types->extensions.size(); ++i) {
    const std::vector<base::string16>& inner_exts = file_types->extensions[i];
    base::string16 ext_string;
    for (size_t j = 0; j < inner_exts.size(); ++j) {
      if (!ext_string.empty())
        ext_string.push_back(L';');
      ext_string.append(L"*.");
      ext_string.append(inner_exts[j]);
    }
    exts.push_back(ext_string);
  }
  return FormatFilterForExtensions(
      exts,
      file_types->extension_description_overrides,
      file_types->include_all_files);
}

}  // namespace

namespace ui {

// This function takes the output of a SaveAs dialog: a filename, a filter and
// the extension originally suggested to the user (shown in the dialog box) and
// returns back the filename with the appropriate extension tacked on. If the
// user requests an unknown extension and is not using the 'All files' filter,
// the suggested extension will be appended, otherwise we will leave the
// filename unmodified. |filename| should contain the filename selected in the
// SaveAs dialog box and may include the path, |filter_selected| should be
// '*.something', for example '*.*' or it can be blank (which is treated as
// *.*). |suggested_ext| should contain the extension without the dot (.) in
// front, for example 'jpg'.
std::wstring AppendExtensionIfNeeded(
    const std::wstring& filename,
    const std::wstring& filter_selected,
    const std::wstring& suggested_ext) {
  DCHECK(!filename.empty());
  std::wstring return_value = filename;

  // If we wanted a specific extension, but the user's filename deleted it or
  // changed it to something that the system doesn't understand, re-append.
  // Careful: Checking net::GetMimeTypeFromExtension() will only find
  // extensions with a known MIME type, which many "known" extensions on Windows
  // don't have.  So we check directly for the "known extension" registry key.
  std::wstring file_extension(
      GetExtensionWithoutLeadingDot(base::FilePath(filename).Extension()));
  std::wstring key(L"." + file_extension);
  if (!(filter_selected.empty() || filter_selected == L"*.*") &&
      !base::win::RegKey(HKEY_CLASSES_ROOT, key.c_str(), KEY_READ).Valid() &&
      file_extension != suggested_ext) {
    if (return_value[return_value.length() - 1] != L'.')
      return_value.append(L".");
    return_value.append(suggested_ext);
  }

  // Strip any trailing dots, which Windows doesn't allow.
  size_t index = return_value.find_last_not_of(L'.');
  if (index < return_value.size() - 1)
    return_value.resize(index + 1);

  return return_value;
}

SelectFileDialog* CreateWinSelectFileDialog(
    SelectFileDialog::Listener* listener,
    SelectFilePolicy* policy) {
  return new SelectFileDialogImpl(listener, policy);
}

}  // namespace ui
