// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_API_H_

#include <set>

#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/api/developer_private/entry_picker.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/api/file_system/file_system_api.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/extensions/error_console/error_console.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/extensions/pack_extension_job.h"
#include "chrome/common/extensions/api/developer_private.h"
#include "chrome/common/extensions/webstore_install_result.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs_observer.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/process_manager_observer.h"
#include "extensions/browser/warning_service.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_operation.h"
#include "ui/shell_dialogs/select_file_dialog.h"

class Profile;

namespace extensions {

class EventRouter;
class ExtensionError;
class ExtensionInfoGenerator;
class ExtensionRegistry;
class ExtensionSystem;
class ManagementPolicy;
class ProcessManager;
class RequirementsChecker;

namespace api {

class EntryPicker;
class EntryPickerClient;

}  // namespace api

class DeveloperPrivateEventRouter : public ExtensionRegistryObserver,
                                    public ErrorConsole::Observer,
                                    public ProcessManagerObserver,
                                    public AppWindowRegistry::Observer,
                                    public CommandService::Observer,
                                    public ExtensionActionAPI::Observer,
                                    public ExtensionPrefsObserver,
                                    public ExtensionManagement::Observer,
                                    public WarningService::Observer {
 public:
  explicit DeveloperPrivateEventRouter(Profile* profile);
  ~DeveloperPrivateEventRouter() override;

  // Add or remove an ID to the list of extensions subscribed to events.
  void AddExtensionId(const std::string& extension_id);
  void RemoveExtensionId(const std::string& extension_id);

 private:
  // ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionInfo::Reason reason) override;
  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const Extension* extension,
                            bool is_update) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const Extension* extension,
                              extensions::UninstallReason reason) override;

  // ErrorConsole::Observer:
  void OnErrorAdded(const ExtensionError* error) override;
  void OnErrorsRemoved(const std::set<std::string>& extension_ids) override;

  // ProcessManagerObserver:
  void OnExtensionFrameRegistered(
      const std::string& extension_id,
      content::RenderFrameHost* render_frame_host) override;
  void OnExtensionFrameUnregistered(
      const std::string& extension_id,
      content::RenderFrameHost* render_frame_host) override;

  // AppWindowRegistry::Observer:
  void OnAppWindowAdded(AppWindow* window) override;
  void OnAppWindowRemoved(AppWindow* window) override;

  // CommandService::Observer:
  void OnExtensionCommandAdded(const std::string& extension_id,
                               const Command& added_command) override;
  void OnExtensionCommandRemoved(const std::string& extension_id,
                                 const Command& removed_command) override;

  // ExtensionActionAPI::Observer:
  void OnExtensionActionVisibilityChanged(const std::string& extension_id,
                                          bool is_now_visible) override;

  // ExtensionPrefsObserver:
  void OnExtensionDisableReasonsChanged(const std::string& extension_id,
                                        int disable_reasons) override;

  // ExtensionManagement::Observer:
  void OnExtensionManagementSettingsChanged() override;

  // WarningService::Observer:
  void ExtensionWarningsChanged(
      const ExtensionIdSet& affected_extensions) override;

  // Handles a profile preferance change.
  void OnProfilePrefChanged();

  // Broadcasts an event to all listeners.
  void BroadcastItemStateChanged(api::developer_private::EventType event_type,
                                 const std::string& id);
  void BroadcastItemStateChangedHelper(
      api::developer_private::EventType event_type,
      const std::string& extension_id,
      std::unique_ptr<ExtensionInfoGenerator> info_generator,
      std::vector<api::developer_private::ExtensionInfo> infos);

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;
  ScopedObserver<ErrorConsole, ErrorConsole::Observer> error_console_observer_;
  ScopedObserver<ProcessManager, ProcessManagerObserver>
      process_manager_observer_;
  ScopedObserver<AppWindowRegistry, AppWindowRegistry::Observer>
      app_window_registry_observer_;
  ScopedObserver<ExtensionActionAPI, ExtensionActionAPI::Observer>
      extension_action_api_observer_;
  ScopedObserver<WarningService, WarningService::Observer>
      warning_service_observer_;
  ScopedObserver<ExtensionPrefs, ExtensionPrefsObserver>
      extension_prefs_observer_;
  ScopedObserver<ExtensionManagement, ExtensionManagement::Observer>
      extension_management_observer_;
  ScopedObserver<CommandService, CommandService::Observer>
      command_service_observer_;

  Profile* profile_;

  EventRouter* event_router_;

  // The set of IDs of the Extensions that have subscribed to DeveloperPrivate
  // events. Since the only consumer of the DeveloperPrivate API is currently
  // the Apps Developer Tool (which replaces the chrome://extensions page), we
  // don't want to send information about the subscribing extension in an
  // update. In particular, we want to avoid entering a loop, which could happen
  // when, e.g., the Apps Developer Tool throws an error.
  std::set<std::string> extension_ids_;

  PrefChangeRegistrar pref_change_registrar_;

  base::WeakPtrFactory<DeveloperPrivateEventRouter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DeveloperPrivateEventRouter);
};

// The profile-keyed service that manages the DeveloperPrivate API.
class DeveloperPrivateAPI : public BrowserContextKeyedAPI,
                            public EventRouter::Observer {
 public:
  static BrowserContextKeyedAPIFactory<DeveloperPrivateAPI>*
      GetFactoryInstance();

  // Convenience method to get the DeveloperPrivateAPI for a profile.
  static DeveloperPrivateAPI* Get(content::BrowserContext* context);

  explicit DeveloperPrivateAPI(content::BrowserContext* context);
  ~DeveloperPrivateAPI() override;

  void SetLastUnpackedDirectory(const base::FilePath& path);

  base::FilePath& GetLastUnpackedDirectory() {
    return last_unpacked_directory_;
  }

  // KeyedService implementation
  void Shutdown() override;

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;
  void OnListenerRemoved(const EventListenerInfo& details) override;

  DeveloperPrivateEventRouter* developer_private_event_router() {
    return developer_private_event_router_.get();
  }

 private:
  friend class BrowserContextKeyedAPIFactory<DeveloperPrivateAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "DeveloperPrivateAPI"; }
  static const bool kServiceRedirectedInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

  void RegisterNotifications();

  Profile* profile_;

  // Used to start the load |load_extension_dialog_| in the last directory that
  // was loaded.
  base::FilePath last_unpacked_directory_;

  // Created lazily upon OnListenerAdded.
  std::unique_ptr<DeveloperPrivateEventRouter> developer_private_event_router_;

  DISALLOW_COPY_AND_ASSIGN(DeveloperPrivateAPI);
};

namespace api {

class DeveloperPrivateAPIFunction : public ChromeUIThreadExtensionFunction {
 protected:
  ~DeveloperPrivateAPIFunction() override;

  // Returns the extension with the given |id| from the registry, including
  // all possible extensions (enabled, disabled, terminated, etc).
  const Extension* GetExtensionById(const std::string& id);

  // Returns the extension with the given |id| from the registry, only checking
  // enabled extensions.
  const Extension* GetEnabledExtensionById(const std::string& id);
};

class DeveloperPrivateAutoUpdateFunction : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.autoUpdate",
                             DEVELOPERPRIVATE_AUTOUPDATE)

 protected:
  ~DeveloperPrivateAutoUpdateFunction() override;
  ResponseAction Run() override;
};

class DeveloperPrivateGetItemsInfoFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.getItemsInfo",
                             DEVELOPERPRIVATE_GETITEMSINFO)
  DeveloperPrivateGetItemsInfoFunction();

 private:
  ~DeveloperPrivateGetItemsInfoFunction() override;
  ResponseAction Run() override;

  void OnInfosGenerated(
      std::vector<api::developer_private::ExtensionInfo> infos);

  std::unique_ptr<ExtensionInfoGenerator> info_generator_;

  DISALLOW_COPY_AND_ASSIGN(DeveloperPrivateGetItemsInfoFunction);
};

class DeveloperPrivateGetExtensionsInfoFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DeveloperPrivateGetExtensionsInfoFunction();
  DECLARE_EXTENSION_FUNCTION("developerPrivate.getExtensionsInfo",
                             DEVELOPERPRIVATE_GETEXTENSIONSINFO);

 private:
  ~DeveloperPrivateGetExtensionsInfoFunction() override;
  ResponseAction Run() override;

  void OnInfosGenerated(
      std::vector<api::developer_private::ExtensionInfo> infos);

  std::unique_ptr<ExtensionInfoGenerator> info_generator_;

  DISALLOW_COPY_AND_ASSIGN(DeveloperPrivateGetExtensionsInfoFunction);
};

class DeveloperPrivateGetExtensionInfoFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DeveloperPrivateGetExtensionInfoFunction();
  DECLARE_EXTENSION_FUNCTION("developerPrivate.getExtensionInfo",
                             DEVELOPERPRIVATE_GETEXTENSIONINFO);

 private:
  ~DeveloperPrivateGetExtensionInfoFunction() override;
  ResponseAction Run() override;

  void OnInfosGenerated(
      std::vector<api::developer_private::ExtensionInfo> infos);

  std::unique_ptr<ExtensionInfoGenerator> info_generator_;

  DISALLOW_COPY_AND_ASSIGN(DeveloperPrivateGetExtensionInfoFunction);
};

class DeveloperPrivateGetProfileConfigurationFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.getProfileConfiguration",
                             DEVELOPERPRIVATE_GETPROFILECONFIGURATION);

 private:
  ~DeveloperPrivateGetProfileConfigurationFunction() override;
  ResponseAction Run() override;
};

class DeveloperPrivateUpdateProfileConfigurationFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.updateProfileConfiguration",
                             DEVELOPERPRIVATE_UPDATEPROFILECONFIGURATION);

 private:
  ~DeveloperPrivateUpdateProfileConfigurationFunction() override;
  ResponseAction Run() override;
};

class DeveloperPrivateUpdateExtensionConfigurationFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.updateExtensionConfiguration",
                             DEVELOPERPRIVATE_UPDATEEXTENSIONCONFIGURATION);

 protected:
  ~DeveloperPrivateUpdateExtensionConfigurationFunction() override;
  ResponseAction Run() override;
};

class DeveloperPrivateReloadFunction : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.reload",
                             DEVELOPERPRIVATE_RELOAD);

 protected:
  ~DeveloperPrivateReloadFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class DeveloperPrivateShowPermissionsDialogFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.showPermissionsDialog",
                             DEVELOPERPRIVATE_PERMISSIONS);
  DeveloperPrivateShowPermissionsDialogFunction();

 protected:
  // DeveloperPrivateAPIFunction:
  ~DeveloperPrivateShowPermissionsDialogFunction() override;
  ResponseAction Run() override;

  void Finish();

  DISALLOW_COPY_AND_ASSIGN(DeveloperPrivateShowPermissionsDialogFunction);
};

class DeveloperPrivateChooseEntryFunction : public UIThreadExtensionFunction,
                                            public EntryPickerClient {
 protected:
  ~DeveloperPrivateChooseEntryFunction() override;
  bool ShowPicker(ui::SelectFileDialog::Type picker_type,
                  const base::string16& select_title,
                  const ui::SelectFileDialog::FileTypeInfo& info,
                  int file_type_index);
};


class DeveloperPrivateLoadUnpackedFunction
    : public DeveloperPrivateChooseEntryFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.loadUnpacked",
                             DEVELOPERPRIVATE_LOADUNPACKED);
  DeveloperPrivateLoadUnpackedFunction();

 protected:
  ~DeveloperPrivateLoadUnpackedFunction() override;
  ResponseAction Run() override;

  // EntryPickerClient:
  void FileSelected(const base::FilePath& path) override;
  void FileSelectionCanceled() override;

  // Callback for the UnpackedLoader.
  void OnLoadComplete(const Extension* extension,
                      const base::FilePath& file_path,
                      const std::string& error);

 private:
  // Whether or not we should fail quietly in the event of a load error.
  bool fail_quietly_;
};

class DeveloperPrivateChoosePathFunction
    : public DeveloperPrivateChooseEntryFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.choosePath",
                             DEVELOPERPRIVATE_CHOOSEPATH);

 protected:
  ~DeveloperPrivateChoosePathFunction() override;
  ResponseAction Run() override;

  // EntryPickerClient:
  void FileSelected(const base::FilePath& path) override;
  void FileSelectionCanceled() override;
};

class DeveloperPrivatePackDirectoryFunction
    : public DeveloperPrivateAPIFunction,
      public PackExtensionJob::Client {

 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.packDirectory",
                             DEVELOPERPRIVATE_PACKDIRECTORY);

  DeveloperPrivatePackDirectoryFunction();

  // ExtensionPackJob::Client implementation.
  void OnPackSuccess(const base::FilePath& crx_file,
                     const base::FilePath& key_file) override;
  void OnPackFailure(const std::string& error,
                     ExtensionCreator::ErrorType error_type) override;

 protected:
  ~DeveloperPrivatePackDirectoryFunction() override;
  ResponseAction Run() override;

 private:
  scoped_refptr<PackExtensionJob> pack_job_;
  std::string item_path_str_;
  std::string key_path_str_;
};

class DeveloperPrivateIsProfileManagedFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.isProfileManaged",
                             DEVELOPERPRIVATE_ISPROFILEMANAGED);

 protected:
  ~DeveloperPrivateIsProfileManagedFunction() override;

  // ExtensionFunction:
  bool RunSync() override;
};

class DeveloperPrivateLoadDirectoryFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.loadDirectory",
                             DEVELOPERPRIVATE_LOADUNPACKEDCROS);

  DeveloperPrivateLoadDirectoryFunction();

 protected:
  ~DeveloperPrivateLoadDirectoryFunction() override;

  // ExtensionFunction:
  bool RunAsync() override;

  bool LoadByFileSystemAPI(const storage::FileSystemURL& directory_url);

  void ClearExistingDirectoryContent(const base::FilePath& project_path);

  void ReadDirectoryByFileSystemAPI(const base::FilePath& project_path,
                                    const base::FilePath& destination_path);

  void ReadDirectoryByFileSystemAPICb(
      const base::FilePath& project_path,
      const base::FilePath& destination_path,
      base::File::Error result,
      const storage::FileSystemOperation::FileEntryList& file_list,
      bool has_more);

  void SnapshotFileCallback(
      const base::FilePath& target_path,
      base::File::Error result,
      const base::File::Info& file_info,
      const base::FilePath& platform_path,
      const scoped_refptr<storage::ShareableFileReference>& file_ref);

  void CopyFile(const base::FilePath& src_path,
                const base::FilePath& dest_path);

  void Load();

  scoped_refptr<storage::FileSystemContext> context_;

  // syncfs url representing the root of the folder to be copied.
  std::string project_base_url_;

  // physical path on disc of the folder to be copied.
  base::FilePath project_base_path_;

 private:
  int pending_copy_operations_count_;

  // This is set to false if any of the copyFile operations fail on
  // call of the API. It is returned as a response of the API call.
  bool success_;
};

class DeveloperPrivateRequestFileSourceFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.requestFileSource",
                             DEVELOPERPRIVATE_REQUESTFILESOURCE);
  DeveloperPrivateRequestFileSourceFunction();

 protected:
  ~DeveloperPrivateRequestFileSourceFunction() override;
  ResponseAction Run() override;

 private:
  void Finish(const std::string& file_contents);

  std::unique_ptr<api::developer_private::RequestFileSource::Params> params_;
};

class DeveloperPrivateOpenDevToolsFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.openDevTools",
                             DEVELOPERPRIVATE_OPENDEVTOOLS);
  DeveloperPrivateOpenDevToolsFunction();

 protected:
  ~DeveloperPrivateOpenDevToolsFunction() override;
  ResponseAction Run() override;
};

class DeveloperPrivateDeleteExtensionErrorsFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.deleteExtensionErrors",
                             DEVELOPERPRIVATE_DELETEEXTENSIONERRORS);

 protected:
  ~DeveloperPrivateDeleteExtensionErrorsFunction() override;
  ResponseAction Run() override;
};

class DeveloperPrivateRepairExtensionFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.repairExtension",
                             DEVELOPERPRIVATE_REPAIREXTENSION);

 protected:
  ~DeveloperPrivateRepairExtensionFunction() override;
  ResponseAction Run() override;

  void OnReinstallComplete(bool success,
                           const std::string& error,
                           webstore_install::Result result);
};

class DeveloperPrivateShowOptionsFunction : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.showOptions",
                             DEVELOPERPRIVATE_SHOWOPTIONS);

 protected:
  ~DeveloperPrivateShowOptionsFunction() override;
  ResponseAction Run() override;
};

class DeveloperPrivateShowPathFunction : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.showPath",
                             DEVELOPERPRIVATE_SHOWPATH);

 protected:
  ~DeveloperPrivateShowPathFunction() override;
  ResponseAction Run() override;
};

class DeveloperPrivateSetShortcutHandlingSuspendedFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.setShortcutHandlingSuspended",
                             DEVELOPERPRIVATE_SETSHORTCUTHANDLINGSUSPENDED);

 protected:
  ~DeveloperPrivateSetShortcutHandlingSuspendedFunction() override;
  ResponseAction Run() override;
};

class DeveloperPrivateUpdateExtensionCommandFunction
    : public DeveloperPrivateAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("developerPrivate.updateExtensionCommand",
                             DEVELOPERPRIVATE_UPDATEEXTENSIONCOMMAND);

 protected:
  ~DeveloperPrivateUpdateExtensionCommandFunction() override;
  ResponseAction Run() override;
};

}  // namespace api

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_DEVELOPER_PRIVATE_API_H_
