// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_FILE_SYSTEM_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_FILE_SYSTEM_API_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/common/extensions/api/file_system.h"
#include "extensions/browser/extension_function.h"
#include "ui/base/ui_base_types.h"
#include "ui/shell_dialogs/select_file_dialog.h"

#if defined(OS_CHROMEOS)
namespace file_manager {
class Volume;
}  // namespace file_manager
#endif

namespace extensions {
class ExtensionPrefs;
class ScopedSkipRequestFileSystemDialog;

namespace file_system_api {

// Methods to get and set the path of the directory containing the last file
// chosen by the user in response to a chrome.fileSystem.chooseEntry() call for
// the given extension.

// Returns an empty path on failure.
base::FilePath GetLastChooseEntryDirectory(const ExtensionPrefs* prefs,
                                           const std::string& extension_id);

void SetLastChooseEntryDirectory(ExtensionPrefs* prefs,
                                 const std::string& extension_id,
                                 const base::FilePath& path);

#if defined(OS_CHROMEOS)
// Dispatches an event about a mounted on unmounted volume in the system to
// each extension which can request it.
void DispatchVolumeListChangeEvent(Profile* profile);

// Requests consent for the chrome.fileSystem.requestFileSystem() method.
// Interaction with UI and environmental checks (kiosk mode, whitelist) are
// provided by a delegate: ConsentProviderDelegate. For testing, it is
// TestingConsentProviderDelegate.
class ConsentProvider {
 public:
  enum Consent { CONSENT_GRANTED, CONSENT_REJECTED, CONSENT_IMPOSSIBLE };
  typedef base::Callback<void(Consent)> ConsentCallback;
  typedef base::Callback<void(ui::DialogButton)> ShowDialogCallback;

  // Interface for delegating user interaction for granting permissions.
  class DelegateInterface {
   public:
    // Shows a dialog for granting permissions.
    virtual void ShowDialog(const Extension& extension,
                            const base::WeakPtr<file_manager::Volume>& volume,
                            bool writable,
                            const ShowDialogCallback& callback) = 0;

    // Shows a notification about permissions automatically granted access.
    virtual void ShowNotification(
        const Extension& extension,
        const base::WeakPtr<file_manager::Volume>& volume,
        bool writable) = 0;

    // Checks if the extension was launched in auto-launch kiosk mode.
    virtual bool IsAutoLaunched(const Extension& extension) = 0;

    // Checks if the extension is a whitelisted component extension or app.
    virtual bool IsWhitelistedComponent(const Extension& extension) = 0;
  };

  explicit ConsentProvider(DelegateInterface* delegate);
  ~ConsentProvider();

  // Requests consent for granting |writable| permissions to the |volume|
  // volume by the |extension|. Must be called only if the extension is
  // grantable, which can be checked with IsGrantable().
  void RequestConsent(const Extension& extension,
                      const base::WeakPtr<file_manager::Volume>& volume,
                      bool writable,
                      const ConsentCallback& callback);

  // Checks whether the |extension| can be granted access.
  bool IsGrantable(const Extension& extension);

 private:
  DelegateInterface* const delegate_;

  // Converts the clicked button to a consent result and passes it via the
  // |callback|.
  void DialogResultToConsent(const ConsentCallback& callback,
                             ui::DialogButton button);

  DISALLOW_COPY_AND_ASSIGN(ConsentProvider);
};

// Handles interaction with user as well as environment checks (whitelists,
// context of running extensions) for ConsentProvider.
class ConsentProviderDelegate : public ConsentProvider::DelegateInterface {
 public:
  ConsentProviderDelegate(Profile* profile, content::RenderFrameHost* host);
  ~ConsentProviderDelegate();

 private:
  friend ScopedSkipRequestFileSystemDialog;

  // Sets a fake result for the user consent dialog. If ui::DIALOG_BUTTON_NONE
  // then disabled.
  static void SetAutoDialogButtonForTest(ui::DialogButton button);

  // ConsentProvider::DelegateInterface overrides:
  void ShowDialog(const Extension& extension,
                  const base::WeakPtr<file_manager::Volume>& volume,
                  bool writable,
                  const file_system_api::ConsentProvider::ShowDialogCallback&
                      callback) override;
  void ShowNotification(const Extension& extension,
                        const base::WeakPtr<file_manager::Volume>& volume,
                        bool writable) override;
  bool IsAutoLaunched(const Extension& extension) override;
  bool IsWhitelistedComponent(const Extension& extension) override;

  Profile* const profile_;
  content::RenderFrameHost* const host_;

  DISALLOW_COPY_AND_ASSIGN(ConsentProviderDelegate);
};
#endif

}  // namespace file_system_api

class FileSystemGetDisplayPathFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystem.getDisplayPath",
                             FILESYSTEM_GETDISPLAYPATH)

 protected:
  ~FileSystemGetDisplayPathFunction() override {}
  ResponseAction Run() override;
};

class FileSystemEntryFunction : public ChromeAsyncExtensionFunction {
 protected:
  FileSystemEntryFunction();

  ~FileSystemEntryFunction() override {}

  // This is called when writable file entries are being returned. The function
  // will ensure the files exist, creating them if necessary, and also check
  // that none of the files are links. If it succeeds it proceeds to
  // RegisterFileSystemsAndSendResponse, otherwise to HandleWritableFileError.
  void PrepareFilesForWritableApp(const std::vector<base::FilePath>& path);

  // This will finish the choose file process. This is either called directly
  // from FilesSelected, or from WritableFileChecker. It is called on the UI
  // thread.
  void RegisterFileSystemsAndSendResponse(
      const std::vector<base::FilePath>& path);

  // Creates a result dictionary.
  std::unique_ptr<base::DictionaryValue> CreateResult();

  // Adds an entry to the result dictionary.
  void AddEntryToResult(const base::FilePath& path,
                        const std::string& id_override,
                        base::DictionaryValue* result);

  // called on the UI thread if there is a problem checking a writable file.
  void HandleWritableFileError(const base::FilePath& error_path);

  // Whether multiple entries have been requested.
  bool multiple_;

  // Whether a directory has been requested.
  bool is_directory_;
};

class FileSystemGetWritableEntryFunction : public FileSystemEntryFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystem.getWritableEntry",
                             FILESYSTEM_GETWRITABLEENTRY)

 protected:
  ~FileSystemGetWritableEntryFunction() override {}
  bool RunAsync() override;

 private:
  void CheckPermissionAndSendResponse();
  void SetIsDirectoryOnFileThread();

  // The path to the file for which a writable entry has been requested.
  base::FilePath path_;
};

class FileSystemIsWritableEntryFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystem.isWritableEntry",
                             FILESYSTEM_ISWRITABLEENTRY)

 protected:
  ~FileSystemIsWritableEntryFunction() override {}
  ResponseAction Run() override;
};

class FileSystemChooseEntryFunction : public FileSystemEntryFunction {
 public:
  // Allow picker UI to be skipped in testing.
  static void SkipPickerAndAlwaysSelectPathForTest(base::FilePath* path);
  static void SkipPickerAndAlwaysSelectPathsForTest(
      std::vector<base::FilePath>* paths);
  static void SkipPickerAndSelectSuggestedPathForTest();
  static void SkipPickerAndAlwaysCancelForTest();
  static void StopSkippingPickerForTest();
  // Allow directory access confirmation UI to be skipped in testing.
  static void SkipDirectoryConfirmationForTest();
  static void AutoCancelDirectoryConfirmationForTest();
  static void StopSkippingDirectoryConfirmationForTest();
  // Call this with the directory for test file paths. On Chrome OS, accessed
  // path needs to be explicitly registered for smooth integration with Google
  // Drive support.
  static void RegisterTempExternalFileSystemForTest(const std::string& name,
                                                    const base::FilePath& path);
  DECLARE_EXTENSION_FUNCTION("fileSystem.chooseEntry", FILESYSTEM_CHOOSEENTRY)

  typedef std::vector<api::file_system::AcceptOption> AcceptOptions;

  static void BuildFileTypeInfo(
      ui::SelectFileDialog::FileTypeInfo* file_type_info,
      const base::FilePath::StringType& suggested_extension,
      const AcceptOptions* accepts,
      const bool* acceptsAllTypes);
  static void BuildSuggestion(const std::string* opt_name,
                              base::FilePath* suggested_name,
                              base::FilePath::StringType* suggested_extension);

 protected:
  class FilePicker;

  ~FileSystemChooseEntryFunction() override {}
  bool RunAsync() override;
  void ShowPicker(const ui::SelectFileDialog::FileTypeInfo& file_type_info,
                  ui::SelectFileDialog::Type picker_type);

 private:
  void SetInitialPathAndShowPicker(
      const base::FilePath& previous_path,
      const base::FilePath& suggested_name,
      const ui::SelectFileDialog::FileTypeInfo& file_type_info,
      ui::SelectFileDialog::Type picker_type,
      bool is_path_non_native_directory);

  // FilesSelected and FileSelectionCanceled are called by the file picker.
  void FilesSelected(const std::vector<base::FilePath>& path);
  void FileSelectionCanceled();

  // Check if the chosen directory is or is an ancestor of a sensitive
  // directory. If so, show a dialog to confirm that the user wants to open the
  // directory. Calls OnDirectoryAccessConfirmed if the directory isn't
  // sensitive or the user chooses to open it. Otherwise, calls
  // FileSelectionCanceled.
  void ConfirmDirectoryAccessOnFileThread(
      bool non_native_path,
      const std::vector<base::FilePath>& paths,
      content::WebContents* web_contents);
  void OnDirectoryAccessConfirmed(const std::vector<base::FilePath>& paths);

  base::FilePath initial_path_;
};

class FileSystemRetainEntryFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystem.retainEntry", FILESYSTEM_RETAINENTRY)

 protected:
  ~FileSystemRetainEntryFunction() override {}
  bool RunAsync() override;

 private:
  // Retains the file entry referenced by |entry_id| in apps::SavedFilesService.
  // |entry_id| must refer to an entry in an isolated file system.  |path| is a
  // path of the entry.  |file_info| is base::File::Info of the entry if it can
  // be obtained.
  void RetainFileEntry(const std::string& entry_id,
                       const base::FilePath& path,
                       std::unique_ptr<base::File::Info> file_info);
};

class FileSystemIsRestorableFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystem.isRestorable", FILESYSTEM_ISRESTORABLE)

 protected:
  ~FileSystemIsRestorableFunction() override {}
  ResponseAction Run() override;
};

class FileSystemRestoreEntryFunction : public FileSystemEntryFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystem.restoreEntry", FILESYSTEM_RESTOREENTRY)

 protected:
  ~FileSystemRestoreEntryFunction() override {}
  bool RunAsync() override;
};

class FileSystemObserveDirectoryFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystem.observeDirectory",
                             FILESYSTEM_OBSERVEDIRECTORY)

 protected:
  ~FileSystemObserveDirectoryFunction() override {}
  ResponseAction Run() override;
};

class FileSystemUnobserveEntryFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystem.unobserveEntry",
                             FILESYSTEM_UNOBSERVEENTRY)

 protected:
  ~FileSystemUnobserveEntryFunction() override {}
  ResponseAction Run() override;
};

class FileSystemGetObservedEntriesFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystem.getObservedEntries",
                             FILESYSTEM_GETOBSERVEDENTRIES);

 protected:
  ~FileSystemGetObservedEntriesFunction() override {}
  ResponseAction Run() override;
};

#if !defined(OS_CHROMEOS)
// Stub for non Chrome OS operating systems.
class FileSystemRequestFileSystemFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystem.requestFileSystem",
                             FILESYSTEM_REQUESTFILESYSTEM);

 protected:
  ~FileSystemRequestFileSystemFunction() override {}

  // UIThreadExtensionFunction overrides.
  ExtensionFunction::ResponseAction Run() override;
};

// Stub for non Chrome OS operating systems.
class FileSystemGetVolumeListFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystem.getVolumeList",
                             FILESYSTEM_GETVOLUMELIST);

 protected:
  ~FileSystemGetVolumeListFunction() override {}

  // UIThreadExtensionFunction overrides.
  ExtensionFunction::ResponseAction Run() override;
};

#else
// Requests a file system for the specified volume id.
class FileSystemRequestFileSystemFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystem.requestFileSystem",
                             FILESYSTEM_REQUESTFILESYSTEM)
  FileSystemRequestFileSystemFunction();

 protected:
  ~FileSystemRequestFileSystemFunction() override;

  // UIThreadExtensionFunction overrides.
  ExtensionFunction::ResponseAction Run() override;

 private:
  // Called when a user grants or rejects permissions for the file system
  // access.
  void OnConsentReceived(const base::WeakPtr<file_manager::Volume>& volume,
                         bool writable,
                         file_system_api::ConsentProvider::Consent result);

  ChromeExtensionFunctionDetails chrome_details_;
};

// Requests a list of available volumes.
class FileSystemGetVolumeListFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("fileSystem.getVolumeList",
                             FILESYSTEM_GETVOLUMELIST);
  FileSystemGetVolumeListFunction();

 protected:
  ~FileSystemGetVolumeListFunction() override;

  // UIThreadExtensionFunction overrides.
  ExtensionFunction::ResponseAction Run() override;

 private:
  ChromeExtensionFunctionDetails chrome_details_;
};
#endif

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_FILE_SYSTEM_API_H_
