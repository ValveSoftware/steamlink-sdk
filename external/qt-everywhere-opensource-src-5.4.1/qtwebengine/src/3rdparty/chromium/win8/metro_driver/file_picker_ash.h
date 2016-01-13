// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_METRO_DRIVER_FILE_PICKER_ASH_H_
#define CHROME_BROWSER_UI_METRO_DRIVER_FILE_PICKER_ASH_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/strings/string16.h"

class ChromeAppViewAsh;
struct MetroViewerHostMsg_SaveAsDialogParams;

namespace base {
class FilePath;
}

// Base class for the file pickers.
class FilePickerSessionBase {
 public:
  // Creates a file picker for open_file_name.
  explicit FilePickerSessionBase(ChromeAppViewAsh* app_view,
                                 const base::string16& title,
                                 const base::string16& filter,
                                 const base::FilePath& default_path);

  virtual ~FilePickerSessionBase() {
  }

  // Runs the picker, returns true on success.
  bool Run();

  const base::string16& result() const {
    return result_;
  }

  bool success() const {
    return success_;
  }

 protected:
  // Creates, configures and starts a file picker.
  // If the HRESULT returned is a failure code the file picker has not started,
  // so no callbacks should be expected.
  virtual HRESULT StartFilePicker() = 0;

  // True iff a file picker has successfully finished.
  bool success_;

  // The title of the file picker.
  base::string16 title_;

  // The file type filter.
  base::string16 filter_;

  // The starting directory/file name.
  base::FilePath default_path_;

  // Pointer to the ChromeAppViewAsh instance. We notify the ChromeAppViewAsh
  // instance when the file open/save operations complete.
  ChromeAppViewAsh* app_view_;

  base::string16 result_;

 private:
  // Initiate a file picker, must be called on the main metro thread.
  // Returns true on success.
  bool DoFilePicker();

  DISALLOW_COPY_AND_ASSIGN(FilePickerSessionBase);
};

// Provides functionality to display the open file/multiple open file pickers
// metro dialog.
class OpenFilePickerSession : public FilePickerSessionBase {
 public:
  explicit OpenFilePickerSession(ChromeAppViewAsh* app_view,
                                 const base::string16& title,
                                 const base::string16& filter,
                                 const base::FilePath& default_path,
                                 bool allow_multi_select);

  const std::vector<base::FilePath>& filenames() const {
    return filenames_;
  }

  const bool allow_multi_select() const {
    return allow_multi_select_;
  }

 private:
  virtual HRESULT StartFilePicker() OVERRIDE;

  typedef winfoundtn::IAsyncOperation<winstorage::StorageFile*>
      SingleFileAsyncOp;
  typedef winfoundtn::Collections::IVectorView<
      winstorage::StorageFile*> StorageFileVectorCollection;
  typedef winfoundtn::IAsyncOperation<StorageFileVectorCollection*>
      MultiFileAsyncOp;

  // Called asynchronously when a single file picker is done.
  HRESULT SinglePickerDone(SingleFileAsyncOp* async, AsyncStatus status);

  // Called asynchronously when a multi file picker is done.
  HRESULT MultiPickerDone(MultiFileAsyncOp* async, AsyncStatus status);

  // Composes a multi-file result string suitable for returning to a
  // from a storage file collection.
  static HRESULT ComposeMultiFileResult(StorageFileVectorCollection* files,
                                        base::string16* result);

 private:
  // True if the multi file picker is to be displayed.
  bool allow_multi_select_;
  // If multi select is true then this member contains the list of filenames
  // to be returned back.
  std::vector<base::FilePath> filenames_;

  DISALLOW_COPY_AND_ASSIGN(OpenFilePickerSession);
};

// Provides functionality to display the save file picker.
class SaveFilePickerSession : public FilePickerSessionBase {
 public:
  explicit SaveFilePickerSession(
      ChromeAppViewAsh* app_view,
      const MetroViewerHostMsg_SaveAsDialogParams& params);

  int SaveFilePickerSession::filter_index() const;

 private:
  virtual HRESULT StartFilePicker() OVERRIDE;

  typedef winfoundtn::IAsyncOperation<winstorage::StorageFile*>
      SaveFileAsyncOp;

  // Called asynchronously when the save file picker is done.
  HRESULT FilePickerDone(SaveFileAsyncOp* async, AsyncStatus status);

  int filter_index_;

  DISALLOW_COPY_AND_ASSIGN(SaveFilePickerSession);
};

// Provides functionality to display the folder picker.
class FolderPickerSession : public FilePickerSessionBase {
 public:
  explicit FolderPickerSession(ChromeAppViewAsh* app_view,
                               const base::string16& title);

 private:
  virtual HRESULT StartFilePicker() OVERRIDE;

  typedef winfoundtn::IAsyncOperation<winstorage::StorageFolder*>
      FolderPickerAsyncOp;

  // Called asynchronously when the folder picker is done.
  HRESULT FolderPickerDone(FolderPickerAsyncOp* async, AsyncStatus status);

  DISALLOW_COPY_AND_ASSIGN(FolderPickerSession);
};

#endif  // CHROME_BROWSER_UI_METRO_DRIVER_FILE_PICKER_ASH_H_

