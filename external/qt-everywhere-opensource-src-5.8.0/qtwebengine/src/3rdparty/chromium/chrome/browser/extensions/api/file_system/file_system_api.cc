// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/file_system/file_system_api.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "apps/saved_files_service.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/file_handlers/app_file_handler_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/path_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/apps/directory_access_confirmation_dialog.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/api/file_system.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/granted_file_entry.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "net/base/mime_util.h"
#include "storage/browser/fileapi/external_mount_points.h"
#include "storage/browser/fileapi/file_system_operation_runner.h"
#include "storage/browser/fileapi/isolated_context.h"
#include "storage/common/fileapi/file_system_types.h"
#include "storage/common/fileapi/file_system_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/selected_file_info.h"

#if defined(OS_MACOSX)
#include <CoreFoundation/CoreFoundation.h>
#include "base/mac/foundation_util.h"
#endif

#if defined(OS_CHROMEOS)
#include "base/strings/string16.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/filesystem_api_util.h"
#include "chrome/browser/extensions/api/file_system/request_file_system_dialog_view.h"
#include "chrome/browser/extensions/api/file_system/request_file_system_notification.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/user_manager.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/manifest_handlers/kiosk_mode_info.h"
#include "url/url_constants.h"
#endif

using apps::SavedFileEntry;
using apps::SavedFilesService;
using storage::IsolatedContext;

const char kInvalidCallingPage[] = "Invalid calling page. This function can't "
    "be called from a background page.";
const char kUserCancelled[] = "User cancelled";
const char kWritableFileErrorFormat[] = "Error opening %s";
const char kRequiresFileSystemWriteError[] =
    "Operation requires fileSystem.write permission";
const char kRequiresFileSystemDirectoryError[] =
    "Operation requires fileSystem.directory permission";
const char kMultipleUnsupportedError[] =
    "acceptsMultiple: true is only supported for 'openFile'";
const char kUnknownIdError[] = "Unknown id";

#if !defined(OS_CHROMEOS)
const char kNotSupportedOnCurrentPlatformError[] =
    "Operation not supported on the current platform.";
#else
const char kNotSupportedOnNonKioskSessionError[] =
    "Operation only supported for kiosk apps running in a kiosk session.";
const char kVolumeNotFoundError[] = "Volume not found.";
const char kSecurityError[] = "Security error.";
const char kConsentImpossible[] =
    "Impossible to ask for user consent as there is no app window visible.";

// List of whitelisted component apps and extensions by their ids for
// chrome.fileSystem.requestFileSystem.
const char* const kRequestFileSystemComponentWhitelist[] = {
    file_manager::kFileManagerAppId,
    file_manager::kVideoPlayerAppId,
    file_manager::kGalleryAppId,
    file_manager::kAudioPlayerAppId,
    file_manager::kImageLoaderExtensionId,
    // TODO(mtomasz): Remove this extension id, and add it only for tests.
    "pkplfbidichfdicaijlchgnapepdginl"  // Testing extensions.
};
#endif

namespace extensions {

namespace file_system = api::file_system;
namespace ChooseEntry = file_system::ChooseEntry;

namespace {

bool g_skip_picker_for_test = false;
bool g_use_suggested_path_for_test = false;
base::FilePath* g_path_to_be_picked_for_test;
std::vector<base::FilePath>* g_paths_to_be_picked_for_test;
bool g_skip_directory_confirmation_for_test = false;
bool g_allow_directory_access_for_test = false;

#if defined(OS_CHROMEOS)
ui::DialogButton g_auto_dialog_button_for_test = ui::DIALOG_BUTTON_NONE;
#endif

// Expand the mime-types and extensions provided in an AcceptOption, returning
// them within the passed extension vector. Returns false if no valid types
// were found.
bool GetFileTypesFromAcceptOption(
    const file_system::AcceptOption& accept_option,
    std::vector<base::FilePath::StringType>* extensions,
    base::string16* description) {
  std::set<base::FilePath::StringType> extension_set;
  int description_id = 0;

  if (accept_option.mime_types.get()) {
    std::vector<std::string>* list = accept_option.mime_types.get();
    bool valid_type = false;
    for (std::vector<std::string>::const_iterator iter = list->begin();
         iter != list->end(); ++iter) {
      std::vector<base::FilePath::StringType> inner;
      std::string accept_type = base::ToLowerASCII(*iter);
      net::GetExtensionsForMimeType(accept_type, &inner);
      if (inner.empty())
        continue;

      if (valid_type)
        description_id = 0;  // We already have an accept type with label; if
                             // we find another, give up and use the default.
      else if (accept_type == "image/*")
        description_id = IDS_IMAGE_FILES;
      else if (accept_type == "audio/*")
        description_id = IDS_AUDIO_FILES;
      else if (accept_type == "video/*")
        description_id = IDS_VIDEO_FILES;

      extension_set.insert(inner.begin(), inner.end());
      valid_type = true;
    }
  }

  if (accept_option.extensions.get()) {
    std::vector<std::string>* list = accept_option.extensions.get();
    for (std::vector<std::string>::const_iterator iter = list->begin();
         iter != list->end(); ++iter) {
      std::string extension = base::ToLowerASCII(*iter);
#if defined(OS_WIN)
      extension_set.insert(base::UTF8ToWide(*iter));
#else
      extension_set.insert(*iter);
#endif
    }
  }

  extensions->assign(extension_set.begin(), extension_set.end());
  if (extensions->empty())
    return false;

  if (accept_option.description.get())
    *description = base::UTF8ToUTF16(*accept_option.description.get());
  else if (description_id)
    *description = l10n_util::GetStringUTF16(description_id);

  return true;
}

// Key for the path of the directory of the file last chosen by the user in
// response to a chrome.fileSystem.chooseEntry() call.
const char kLastChooseEntryDirectory[] = "last_choose_file_directory";

const int kGraylistedPaths[] = {
  base::DIR_HOME,
#if defined(OS_WIN)
  base::DIR_PROGRAM_FILES,
  base::DIR_PROGRAM_FILESX86,
  base::DIR_WINDOWS,
#endif
};

typedef base::Callback<void(std::unique_ptr<base::File::Info>)>
    FileInfoOptCallback;

// Passes optional file info to the UI thread depending on |result| and |info|.
void PassFileInfoToUIThread(const FileInfoOptCallback& callback,
                            base::File::Error result,
                            const base::File::Info& info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  std::unique_ptr<base::File::Info> file_info(
      result == base::File::FILE_OK ? new base::File::Info(info) : NULL);
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(callback, base::Passed(&file_info)));
}

// Gets a WebContents instance handle for a platform app hosted in
// |render_frame_host|. If not found, then returns NULL.
content::WebContents* GetWebContentsForRenderFrameHost(
    Profile* profile,
    content::RenderFrameHost* render_frame_host) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  // Check if there is an app window associated with the web contents; if not,
  // return null.
  return AppWindowRegistry::Get(profile)
                 ->GetAppWindowForWebContents(web_contents)
             ? web_contents
             : nullptr;
}

#if defined(OS_CHROMEOS)
// Gets a WebContents instance handle for a current window of a platform app
// with |app_id|. If not found, then returns NULL.
content::WebContents* GetWebContentsForAppId(Profile* profile,
                                             const std::string& app_id) {
  AppWindowRegistry* const registry = AppWindowRegistry::Get(profile);
  DCHECK(registry);
  AppWindow* const app_window = registry->GetCurrentAppWindowForApp(app_id);
  return app_window ? app_window->web_contents() : nullptr;
}

// Fills a list of volumes mounted in the system.
void FillVolumeList(Profile* profile,
                    std::vector<api::file_system::Volume>* result) {
  file_manager::VolumeManager* const volume_manager =
      file_manager::VolumeManager::Get(profile);
  DCHECK(volume_manager);

  const auto& volume_list = volume_manager->GetVolumeList();
  // Convert volume_list to result_volume_list.
  for (const auto& volume : volume_list) {
    api::file_system::Volume result_volume;
    result_volume.volume_id = volume->volume_id();
    result_volume.writable = !volume->is_read_only();
    result->push_back(std::move(result_volume));
  }
}
#endif

}  // namespace

namespace file_system_api {

base::FilePath GetLastChooseEntryDirectory(const ExtensionPrefs* prefs,
                                           const std::string& extension_id) {
  base::FilePath path;
  std::string string_path;
  if (prefs->ReadPrefAsString(extension_id,
                              kLastChooseEntryDirectory,
                              &string_path)) {
    path = base::FilePath::FromUTF8Unsafe(string_path);
  }
  return path;
}

void SetLastChooseEntryDirectory(ExtensionPrefs* prefs,
                                 const std::string& extension_id,
                                 const base::FilePath& path) {
  prefs->UpdateExtensionPref(extension_id,
                             kLastChooseEntryDirectory,
                             base::CreateFilePathValue(path));
}

#if defined(OS_CHROMEOS)
void DispatchVolumeListChangeEvent(Profile* profile) {
  DCHECK(profile);
  EventRouter* const event_router = EventRouter::Get(profile);
  if (!event_router)  // Possible on shutdown.
    return;

  ExtensionRegistry* const registry = ExtensionRegistry::Get(profile);
  if (!registry)  // Possible on shutdown.
    return;

  ConsentProviderDelegate consent_provider_delegate(profile, nullptr);
  ConsentProvider consent_provider(&consent_provider_delegate);
  api::file_system::VolumeListChangedEvent event_args;
  FillVolumeList(profile, &event_args.volumes);
  for (const auto& extension : registry->enabled_extensions()) {
    if (!consent_provider.IsGrantable(*extension.get()))
      continue;
    event_router->DispatchEventToExtension(
        extension->id(),
        base::WrapUnique(new Event(
            events::FILE_SYSTEM_ON_VOLUME_LIST_CHANGED,
            api::file_system::OnVolumeListChanged::kEventName,
            api::file_system::OnVolumeListChanged::Create(event_args))));
  }
}

ConsentProvider::ConsentProvider(DelegateInterface* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

ConsentProvider::~ConsentProvider() {
}

void ConsentProvider::RequestConsent(
    const Extension& extension,
    const base::WeakPtr<file_manager::Volume>& volume,
    bool writable,
    const ConsentCallback& callback) {
  DCHECK(IsGrantable(extension));

  // If a whitelisted component, then no need to ask or inform the user.
  if (extension.location() == Manifest::COMPONENT &&
      delegate_->IsWhitelistedComponent(extension)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, CONSENT_GRANTED));
    return;
  }

  // If auto-launched kiosk app, then no need to ask user either, but show the
  // notification.
  if (delegate_->IsAutoLaunched(extension)) {
    delegate_->ShowNotification(extension, volume, writable);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, CONSENT_GRANTED));
    return;
  }

  // If it's a kiosk app running in manual-launch kiosk session, then show
  // the confirmation dialog.
  if (KioskModeInfo::IsKioskOnly(&extension) &&
      user_manager::UserManager::Get()->IsLoggedInAsKioskApp()) {
    delegate_->ShowDialog(extension, volume, writable,
                          base::Bind(&ConsentProvider::DialogResultToConsent,
                                     base::Unretained(this), callback));
    return;
  }

  NOTREACHED() << "Cannot request consent for non-grantable extensions.";
}

bool ConsentProvider::IsGrantable(const Extension& extension) {
  const bool is_whitelisted_component =
      delegate_->IsWhitelistedComponent(extension);

  const bool is_running_in_kiosk_session =
      KioskModeInfo::IsKioskOnly(&extension) &&
      user_manager::UserManager::Get()->IsLoggedInAsKioskApp();

  return is_whitelisted_component || is_running_in_kiosk_session;
}

void ConsentProvider::DialogResultToConsent(const ConsentCallback& callback,
                                            ui::DialogButton button) {
  switch (button) {
    case ui::DIALOG_BUTTON_NONE:
      callback.Run(CONSENT_IMPOSSIBLE);
      break;
    case ui::DIALOG_BUTTON_OK:
      callback.Run(CONSENT_GRANTED);
      break;
    case ui::DIALOG_BUTTON_CANCEL:
      callback.Run(CONSENT_REJECTED);
      break;
  }
}

ConsentProviderDelegate::ConsentProviderDelegate(Profile* profile,
                                                 content::RenderFrameHost* host)
    : profile_(profile), host_(host) {
  DCHECK(profile_);
}

ConsentProviderDelegate::~ConsentProviderDelegate() {
}

// static
void ConsentProviderDelegate::SetAutoDialogButtonForTest(
    ui::DialogButton button) {
  g_auto_dialog_button_for_test = button;
}

void ConsentProviderDelegate::ShowDialog(
    const Extension& extension,
    const base::WeakPtr<file_manager::Volume>& volume,
    bool writable,
    const file_system_api::ConsentProvider::ShowDialogCallback& callback) {
  DCHECK(host_);
  content::WebContents* const foreground_contents =
      GetWebContentsForRenderFrameHost(profile_, host_);
  // If there is no web contents handle, then the method is most probably
  // executed from a background page. Find an app window to host the dialog.
  content::WebContents* const web_contents =
      foreground_contents ? foreground_contents
                          : GetWebContentsForAppId(profile_, extension.id());
  if (!web_contents) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, ui::DIALOG_BUTTON_NONE));
    return;
  }

  // Short circuit the user consent dialog for tests. This is far from a pretty
  // code design.
  if (g_auto_dialog_button_for_test != ui::DIALOG_BUTTON_NONE) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(callback, g_auto_dialog_button_for_test /* result */));
    return;
  }

  RequestFileSystemDialogView::ShowDialog(web_contents, extension, volume,
                                          writable, base::Bind(callback));
}

void ConsentProviderDelegate::ShowNotification(
    const Extension& extension,
    const base::WeakPtr<file_manager::Volume>& volume,
    bool writable) {
  RequestFileSystemNotification::ShowAutoGrantedNotification(
      profile_, extension, volume, writable);
}

bool ConsentProviderDelegate::IsAutoLaunched(const Extension& extension) {
  chromeos::KioskAppManager::App app_info;
  return chromeos::KioskAppManager::Get()->GetApp(extension.id(), &app_info) &&
         app_info.was_auto_launched_with_zero_delay;
}

bool ConsentProviderDelegate::IsWhitelistedComponent(
    const Extension& extension) {
  for (const auto& whitelisted_id : kRequestFileSystemComponentWhitelist) {
    if (extension.id().compare(whitelisted_id) == 0)
      return true;
  }
  return false;
}

#endif

}  // namespace file_system_api

#if defined(OS_CHROMEOS)
using file_system_api::ConsentProvider;
#endif

bool FileSystemGetDisplayPathFunction::RunSync() {
  std::string filesystem_name;
  std::string filesystem_path;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &filesystem_name));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &filesystem_path));

  base::FilePath file_path;
  if (!app_file_handler_util::ValidateFileEntryAndGetPath(
          filesystem_name, filesystem_path,
          render_frame_host()->GetProcess()->GetID(), &file_path, &error_))
    return false;

  file_path = path_util::PrettifyPath(file_path);
  SetResult(base::MakeUnique<base::StringValue>(file_path.value()));
  return true;
}

FileSystemEntryFunction::FileSystemEntryFunction()
    : multiple_(false), is_directory_(false) {}

void FileSystemEntryFunction::PrepareFilesForWritableApp(
    const std::vector<base::FilePath>& paths) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // TODO(cmihail): Path directory set should be initialized only with the
  // paths that are actually directories, but for now we will consider
  // all paths directories in case is_directory_ is true, otherwise
  // all paths files, as this was the previous logic.
  std::set<base::FilePath> path_directory_set_ =
      is_directory_ ? std::set<base::FilePath>(paths.begin(), paths.end())
                    : std::set<base::FilePath>{};
  app_file_handler_util::PrepareFilesForWritableApp(
      paths, GetProfile(), path_directory_set_,
      base::Bind(&FileSystemEntryFunction::RegisterFileSystemsAndSendResponse,
                 this, paths),
      base::Bind(&FileSystemEntryFunction::HandleWritableFileError, this));
}

void FileSystemEntryFunction::RegisterFileSystemsAndSendResponse(
    const std::vector<base::FilePath>& paths) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!render_frame_host())
    return;

  std::unique_ptr<base::DictionaryValue> result = CreateResult();
  for (const auto& path : paths)
    AddEntryToResult(path, std::string(), result.get());
  SetResult(std::move(result));
  SendResponse(true);
}

std::unique_ptr<base::DictionaryValue> FileSystemEntryFunction::CreateResult() {
  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());
  result->Set("entries", base::MakeUnique<base::ListValue>());
  result->SetBoolean("multiple", multiple_);
  return result;
}

void FileSystemEntryFunction::AddEntryToResult(const base::FilePath& path,
                                               const std::string& id_override,
                                               base::DictionaryValue* result) {
  GrantedFileEntry file_entry = app_file_handler_util::CreateFileEntry(
      GetProfile(),
      extension(),
      render_frame_host()->GetProcess()->GetID(),
      path,
      is_directory_);
  base::ListValue* entries;
  bool success = result->GetList("entries", &entries);
  DCHECK(success);

  std::unique_ptr<base::DictionaryValue> entry(new base::DictionaryValue());
  entry->SetString("fileSystemId", file_entry.filesystem_id);
  entry->SetString("baseName", file_entry.registered_name);
  if (id_override.empty())
    entry->SetString("id", file_entry.id);
  else
    entry->SetString("id", id_override);
  entry->SetBoolean("isDirectory", is_directory_);
  entries->Append(std::move(entry));
}

void FileSystemEntryFunction::HandleWritableFileError(
    const base::FilePath& error_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  error_ = base::StringPrintf(kWritableFileErrorFormat,
                              error_path.BaseName().AsUTF8Unsafe().c_str());
  SendResponse(false);
}

bool FileSystemGetWritableEntryFunction::RunAsync() {
  std::string filesystem_name;
  std::string filesystem_path;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &filesystem_name));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &filesystem_path));

  if (!app_file_handler_util::HasFileSystemWritePermission(extension_.get())) {
    error_ = kRequiresFileSystemWriteError;
    return false;
  }

  if (!app_file_handler_util::ValidateFileEntryAndGetPath(
          filesystem_name, filesystem_path,
          render_frame_host()->GetProcess()->GetID(), &path_, &error_))
    return false;

  content::BrowserThread::PostTaskAndReply(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(
          &FileSystemGetWritableEntryFunction::SetIsDirectoryOnFileThread,
          this),
      base::Bind(
          &FileSystemGetWritableEntryFunction::CheckPermissionAndSendResponse,
          this));
  return true;
}

void FileSystemGetWritableEntryFunction::CheckPermissionAndSendResponse() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (is_directory_ &&
      !extension_->permissions_data()->HasAPIPermission(
          APIPermission::kFileSystemDirectory)) {
    error_ = kRequiresFileSystemDirectoryError;
    SendResponse(false);
  }
  std::vector<base::FilePath> paths;
  paths.push_back(path_);
  PrepareFilesForWritableApp(paths);
}

void FileSystemGetWritableEntryFunction::SetIsDirectoryOnFileThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::FILE);
  if (base::DirectoryExists(path_)) {
    is_directory_ = true;
  }
}

bool FileSystemIsWritableEntryFunction::RunSync() {
  std::string filesystem_name;
  std::string filesystem_path;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &filesystem_name));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &filesystem_path));

  std::string filesystem_id;
  if (!storage::CrackIsolatedFileSystemName(filesystem_name, &filesystem_id)) {
    error_ = app_file_handler_util::kInvalidParameters;
    return false;
  }

  content::ChildProcessSecurityPolicy* policy =
      content::ChildProcessSecurityPolicy::GetInstance();
  int renderer_id = render_frame_host()->GetProcess()->GetID();
  bool is_writable = policy->CanReadWriteFileSystem(renderer_id,
                                                    filesystem_id);

  SetResult(base::MakeUnique<base::FundamentalValue>(is_writable));
  return true;
}

// Handles showing a dialog to the user to ask for the filename for a file to
// save or open.
class FileSystemChooseEntryFunction::FilePicker
    : public ui::SelectFileDialog::Listener {
 public:
  FilePicker(FileSystemChooseEntryFunction* function,
             content::WebContents* web_contents,
             const base::FilePath& suggested_name,
             const ui::SelectFileDialog::FileTypeInfo& file_type_info,
             ui::SelectFileDialog::Type picker_type)
      : function_(function) {
    select_file_dialog_ = ui::SelectFileDialog::Create(
        this, new ChromeSelectFilePolicy(web_contents));
    gfx::NativeWindow owning_window = web_contents ?
        platform_util::GetTopLevel(web_contents->GetNativeView()) :
        NULL;

    if (g_skip_picker_for_test) {
      if (g_use_suggested_path_for_test) {
        content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
            base::Bind(
                &FileSystemChooseEntryFunction::FilePicker::FileSelected,
                base::Unretained(this), suggested_name, 1,
                static_cast<void*>(NULL)));
      } else if (g_path_to_be_picked_for_test) {
        content::BrowserThread::PostTask(
            content::BrowserThread::UI, FROM_HERE,
            base::Bind(
                &FileSystemChooseEntryFunction::FilePicker::FileSelected,
                base::Unretained(this), *g_path_to_be_picked_for_test, 1,
                static_cast<void*>(NULL)));
      } else if (g_paths_to_be_picked_for_test) {
        content::BrowserThread::PostTask(
            content::BrowserThread::UI,
            FROM_HERE,
            base::Bind(
                &FileSystemChooseEntryFunction::FilePicker::MultiFilesSelected,
                base::Unretained(this),
                *g_paths_to_be_picked_for_test,
                static_cast<void*>(NULL)));
      } else {
        content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
            base::Bind(
                &FileSystemChooseEntryFunction::FilePicker::
                    FileSelectionCanceled,
                base::Unretained(this), static_cast<void*>(NULL)));
      }
      return;
    }

    select_file_dialog_->SelectFile(picker_type,
                                    base::string16(),
                                    suggested_name,
                                    &file_type_info,
                                    0,
                                    base::FilePath::StringType(),
                                    owning_window,
                                    NULL);
  }

  ~FilePicker() override {}

 private:
  // ui::SelectFileDialog::Listener implementation.
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override {
    std::vector<base::FilePath> paths;
    paths.push_back(path);
    MultiFilesSelected(paths, params);
  }

  void FileSelectedWithExtraInfo(const ui::SelectedFileInfo& file,
                                 int index,
                                 void* params) override {
    // Normally, file.local_path is used because it is a native path to the
    // local read-only cached file in the case of remote file system like
    // Chrome OS's Google Drive integration. Here, however, |file.file_path| is
    // necessary because we need to create a FileEntry denoting the remote file,
    // not its cache. On other platforms than Chrome OS, they are the same.
    //
    // TODO(kinaba): remove this, once after the file picker implements proper
    // switch of the path treatment depending on the |allowed_paths|.
    FileSelected(file.file_path, index, params);
  }

  void MultiFilesSelected(const std::vector<base::FilePath>& files,
                          void* params) override {
    function_->FilesSelected(files);
    delete this;
  }

  void MultiFilesSelectedWithExtraInfo(
      const std::vector<ui::SelectedFileInfo>& files,
      void* params) override {
    std::vector<base::FilePath> paths;
    for (std::vector<ui::SelectedFileInfo>::const_iterator it = files.begin();
         it != files.end(); ++it) {
      paths.push_back(it->file_path);
    }
    MultiFilesSelected(paths, params);
  }

  void FileSelectionCanceled(void* params) override {
    function_->FileSelectionCanceled();
    delete this;
  }

  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  scoped_refptr<FileSystemChooseEntryFunction> function_;

  DISALLOW_COPY_AND_ASSIGN(FilePicker);
};

void FileSystemChooseEntryFunction::ShowPicker(
    const ui::SelectFileDialog::FileTypeInfo& file_type_info,
    ui::SelectFileDialog::Type picker_type) {
  // TODO(asargent/benwells) - As a short term remediation for crbug.com/179010
  // we're adding the ability for a whitelisted extension to use this API since
  // chrome.fileBrowserHandler.selectFile is ChromeOS-only. Eventually we'd
  // like a better solution and likely this code will go back to being
  // platform-app only.
  content::WebContents* const web_contents =
      extension_->is_platform_app()
          ? GetWebContentsForRenderFrameHost(GetProfile(), render_frame_host())
          : GetAssociatedWebContents();
  if (!web_contents) {
    error_ = kInvalidCallingPage;
    SendResponse(false);
    return;
  }

  // The file picker will hold a reference to this function instance, preventing
  // its destruction (and subsequent sending of the function response) until the
  // user has selected a file or cancelled the picker. At that point, the picker
  // will delete itself, which will also free the function instance.
  new FilePicker(
      this, web_contents, initial_path_, file_type_info, picker_type);
}

// static
void FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathForTest(
    base::FilePath* path) {
  g_skip_picker_for_test = true;
  g_use_suggested_path_for_test = false;
  g_path_to_be_picked_for_test = path;
  g_paths_to_be_picked_for_test = NULL;
}

void FileSystemChooseEntryFunction::SkipPickerAndAlwaysSelectPathsForTest(
    std::vector<base::FilePath>* paths) {
  g_skip_picker_for_test = true;
  g_use_suggested_path_for_test = false;
  g_paths_to_be_picked_for_test = paths;
}

// static
void FileSystemChooseEntryFunction::SkipPickerAndSelectSuggestedPathForTest() {
  g_skip_picker_for_test = true;
  g_use_suggested_path_for_test = true;
  g_path_to_be_picked_for_test = NULL;
  g_paths_to_be_picked_for_test = NULL;
}

// static
void FileSystemChooseEntryFunction::SkipPickerAndAlwaysCancelForTest() {
  g_skip_picker_for_test = true;
  g_use_suggested_path_for_test = false;
  g_path_to_be_picked_for_test = NULL;
  g_paths_to_be_picked_for_test = NULL;
}

// static
void FileSystemChooseEntryFunction::StopSkippingPickerForTest() {
  g_skip_picker_for_test = false;
}

// static
void FileSystemChooseEntryFunction::SkipDirectoryConfirmationForTest() {
  g_skip_directory_confirmation_for_test = true;
  g_allow_directory_access_for_test = true;
}

// static
void FileSystemChooseEntryFunction::AutoCancelDirectoryConfirmationForTest() {
  g_skip_directory_confirmation_for_test = true;
  g_allow_directory_access_for_test = false;
}

// static
void FileSystemChooseEntryFunction::StopSkippingDirectoryConfirmationForTest() {
  g_skip_directory_confirmation_for_test = false;
}

// static
void FileSystemChooseEntryFunction::RegisterTempExternalFileSystemForTest(
    const std::string& name, const base::FilePath& path) {
  // For testing on Chrome OS, where to deal with remote and local paths
  // smoothly, all accessed paths need to be registered in the list of
  // external mount points.
  storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      name,
      storage::kFileSystemTypeNativeLocal,
      storage::FileSystemMountOption(),
      path);
}

void FileSystemChooseEntryFunction::FilesSelected(
    const std::vector<base::FilePath>& paths) {
  DCHECK(!paths.empty());
  base::FilePath last_choose_directory;
  if (is_directory_) {
    last_choose_directory = paths[0];
  } else {
    last_choose_directory = paths[0].DirName();
  }
  file_system_api::SetLastChooseEntryDirectory(
      ExtensionPrefs::Get(GetProfile()),
      extension()->id(),
      last_choose_directory);
  if (is_directory_) {
    // Get the WebContents for the app window to be the parent window of the
    // confirmation dialog if necessary.
    content::WebContents* const web_contents =
        GetWebContentsForRenderFrameHost(GetProfile(), render_frame_host());
    if (!web_contents) {
      error_ = kInvalidCallingPage;
      SendResponse(false);
      return;
    }

    DCHECK_EQ(paths.size(), 1u);
    bool non_native_path = false;
#if defined(OS_CHROMEOS)
    non_native_path =
        file_manager::util::IsUnderNonNativeLocalPath(GetProfile(), paths[0]);
#endif

    content::BrowserThread::PostTask(
        content::BrowserThread::FILE,
        FROM_HERE,
        base::Bind(
            &FileSystemChooseEntryFunction::ConfirmDirectoryAccessOnFileThread,
            this,
            non_native_path,
            paths,
            web_contents));
    return;
  }

  OnDirectoryAccessConfirmed(paths);
}

void FileSystemChooseEntryFunction::FileSelectionCanceled() {
  error_ = kUserCancelled;
  SendResponse(false);
}

void FileSystemChooseEntryFunction::ConfirmDirectoryAccessOnFileThread(
    bool non_native_path,
    const std::vector<base::FilePath>& paths,
    content::WebContents* web_contents) {
  const base::FilePath check_path =
      non_native_path ? paths[0] : base::MakeAbsoluteFilePath(paths[0]);
  if (check_path.empty()) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI,
        FROM_HERE,
        base::Bind(&FileSystemChooseEntryFunction::FileSelectionCanceled,
                   this));
    return;
  }

  for (size_t i = 0; i < arraysize(kGraylistedPaths); i++) {
    base::FilePath graylisted_path;
    if (PathService::Get(kGraylistedPaths[i], &graylisted_path) &&
        (check_path == graylisted_path ||
         check_path.IsParent(graylisted_path))) {
      if (g_skip_directory_confirmation_for_test) {
        if (g_allow_directory_access_for_test) {
          break;
        } else {
          content::BrowserThread::PostTask(
              content::BrowserThread::UI,
              FROM_HERE,
              base::Bind(&FileSystemChooseEntryFunction::FileSelectionCanceled,
                         this));
        }
        return;
      }

      content::BrowserThread::PostTask(
          content::BrowserThread::UI,
          FROM_HERE,
          base::Bind(
              CreateDirectoryAccessConfirmationDialog,
              app_file_handler_util::HasFileSystemWritePermission(
                  extension_.get()),
              base::UTF8ToUTF16(extension_->name()),
              web_contents,
              base::Bind(
                  &FileSystemChooseEntryFunction::OnDirectoryAccessConfirmed,
                  this,
                  paths),
              base::Bind(&FileSystemChooseEntryFunction::FileSelectionCanceled,
                         this)));
      return;
    }
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&FileSystemChooseEntryFunction::OnDirectoryAccessConfirmed,
                 this, paths));
}

void FileSystemChooseEntryFunction::OnDirectoryAccessConfirmed(
    const std::vector<base::FilePath>& paths) {
  if (app_file_handler_util::HasFileSystemWritePermission(extension_.get())) {
    PrepareFilesForWritableApp(paths);
    return;
  }

  // Don't need to check the file, it's for reading.
  RegisterFileSystemsAndSendResponse(paths);
}

void FileSystemChooseEntryFunction::BuildFileTypeInfo(
    ui::SelectFileDialog::FileTypeInfo* file_type_info,
    const base::FilePath::StringType& suggested_extension,
    const AcceptOptions* accepts,
    const bool* acceptsAllTypes) {
  file_type_info->include_all_files = true;
  if (acceptsAllTypes)
    file_type_info->include_all_files = *acceptsAllTypes;

  bool need_suggestion = !file_type_info->include_all_files &&
                         !suggested_extension.empty();

  if (accepts) {
    for (const file_system::AcceptOption& option : *accepts) {
      base::string16 description;
      std::vector<base::FilePath::StringType> extensions;

      if (!GetFileTypesFromAcceptOption(option, &extensions, &description))
        continue;  // No extensions were found.

      file_type_info->extensions.push_back(extensions);
      file_type_info->extension_description_overrides.push_back(description);

      // If we still need to find suggested_extension, hunt for it inside the
      // extensions returned from GetFileTypesFromAcceptOption.
      if (need_suggestion && std::find(extensions.begin(),
              extensions.end(), suggested_extension) != extensions.end()) {
        need_suggestion = false;
      }
    }
  }

  // If there's nothing in our accepted extension list or we couldn't find the
  // suggested extension required, then default to accepting all types.
  if (file_type_info->extensions.empty() || need_suggestion)
    file_type_info->include_all_files = true;
}

void FileSystemChooseEntryFunction::BuildSuggestion(
    const std::string *opt_name,
    base::FilePath* suggested_name,
    base::FilePath::StringType* suggested_extension) {
  if (opt_name) {
    *suggested_name = base::FilePath::FromUTF8Unsafe(*opt_name);

    // Don't allow any path components; shorten to the base name. This should
    // result in a relative path, but in some cases may not. Clear the
    // suggestion for safety if this is the case.
    *suggested_name = suggested_name->BaseName();
    if (suggested_name->IsAbsolute())
      *suggested_name = base::FilePath();

    *suggested_extension = suggested_name->Extension();
    if (!suggested_extension->empty())
      suggested_extension->erase(suggested_extension->begin());  // drop the .
  }
}

void FileSystemChooseEntryFunction::SetInitialPathAndShowPicker(
    const base::FilePath& previous_path,
    const base::FilePath& suggested_name,
    const ui::SelectFileDialog::FileTypeInfo& file_type_info,
    ui::SelectFileDialog::Type picker_type,
    bool is_previous_path_directory) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (is_previous_path_directory) {
    initial_path_ = previous_path.Append(suggested_name);
  } else {
    base::FilePath documents_dir;
    if (PathService::Get(chrome::DIR_USER_DOCUMENTS, &documents_dir)) {
      initial_path_ = documents_dir.Append(suggested_name);
    } else {
      initial_path_ = suggested_name;
    }
  }
  ShowPicker(file_type_info, picker_type);
}

bool FileSystemChooseEntryFunction::RunAsync() {
  std::unique_ptr<ChooseEntry::Params> params(
      ChooseEntry::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  base::FilePath suggested_name;
  ui::SelectFileDialog::FileTypeInfo file_type_info;
  ui::SelectFileDialog::Type picker_type =
      ui::SelectFileDialog::SELECT_OPEN_FILE;

  file_system::ChooseEntryOptions* options = params->options.get();
  if (options) {
    multiple_ = options->accepts_multiple && *options->accepts_multiple;
    if (multiple_)
      picker_type = ui::SelectFileDialog::SELECT_OPEN_MULTI_FILE;

    if (options->type == file_system::CHOOSE_ENTRY_TYPE_OPENWRITABLEFILE &&
        !app_file_handler_util::HasFileSystemWritePermission(
            extension_.get())) {
      error_ = kRequiresFileSystemWriteError;
      return false;
    } else if (options->type == file_system::CHOOSE_ENTRY_TYPE_SAVEFILE) {
      if (!app_file_handler_util::HasFileSystemWritePermission(
              extension_.get())) {
        error_ = kRequiresFileSystemWriteError;
        return false;
      }
      if (multiple_) {
        error_ = kMultipleUnsupportedError;
        return false;
      }
      picker_type = ui::SelectFileDialog::SELECT_SAVEAS_FILE;
    } else if (options->type == file_system::CHOOSE_ENTRY_TYPE_OPENDIRECTORY) {
      is_directory_ = true;
      if (!extension_->permissions_data()->HasAPIPermission(
              APIPermission::kFileSystemDirectory)) {
        error_ = kRequiresFileSystemDirectoryError;
        return false;
      }
      if (multiple_) {
        error_ = kMultipleUnsupportedError;
        return false;
      }
      picker_type = ui::SelectFileDialog::SELECT_FOLDER;
    }

    base::FilePath::StringType suggested_extension;
    BuildSuggestion(options->suggested_name.get(), &suggested_name,
        &suggested_extension);

    BuildFileTypeInfo(&file_type_info, suggested_extension,
        options->accepts.get(), options->accepts_all_types.get());
  }

  file_type_info.allowed_paths = ui::SelectFileDialog::FileTypeInfo::ANY_PATH;

  base::FilePath previous_path = file_system_api::GetLastChooseEntryDirectory(
      ExtensionPrefs::Get(GetProfile()), extension()->id());

  if (previous_path.empty()) {
    SetInitialPathAndShowPicker(previous_path, suggested_name, file_type_info,
                                picker_type, false);
    return true;
  }

  base::Callback<void(bool)> set_initial_path_callback = base::Bind(
      &FileSystemChooseEntryFunction::SetInitialPathAndShowPicker, this,
      previous_path, suggested_name, file_type_info, picker_type);

// Check whether the |previous_path| is a non-native directory.
#if defined(OS_CHROMEOS)
  if (file_manager::util::IsUnderNonNativeLocalPath(GetProfile(),
                                                    previous_path)) {
    file_manager::util::IsNonNativeLocalPathDirectory(
        GetProfile(), previous_path, set_initial_path_callback);
    return true;
  }
#endif
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&base::DirectoryExists, previous_path),
      set_initial_path_callback);

  return true;
}

bool FileSystemRetainEntryFunction::RunAsync() {
  std::string entry_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &entry_id));
  SavedFilesService* saved_files_service = SavedFilesService::Get(GetProfile());
  // Add the file to the retain list if it is not already on there.
  if (!saved_files_service->IsRegistered(extension_->id(), entry_id)) {
    std::string filesystem_name;
    std::string filesystem_path;
    base::FilePath path;
    EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &filesystem_name));
    EXTENSION_FUNCTION_VALIDATE(args_->GetString(2, &filesystem_path));
    if (!app_file_handler_util::ValidateFileEntryAndGetPath(
            filesystem_name, filesystem_path,
            render_frame_host()->GetProcess()->GetID(), &path, &error_)) {
      return false;
    }

    std::string filesystem_id;
    if (!storage::CrackIsolatedFileSystemName(filesystem_name, &filesystem_id))
      return false;

    const GURL site = util::GetSiteForExtensionId(extension_id(), GetProfile());
    storage::FileSystemContext* const context =
        content::BrowserContext::GetStoragePartitionForSite(GetProfile(), site)
            ->GetFileSystemContext();
    const storage::FileSystemURL url = context->CreateCrackedFileSystemURL(
        site,
        storage::kFileSystemTypeIsolated,
        IsolatedContext::GetInstance()
            ->CreateVirtualRootPath(filesystem_id)
            .Append(base::FilePath::FromUTF8Unsafe(filesystem_path)));

    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(
            base::IgnoreResult(
                &storage::FileSystemOperationRunner::GetMetadata),
            context->operation_runner()->AsWeakPtr(), url,
            storage::FileSystemOperation::GET_METADATA_FIELD_IS_DIRECTORY,
            base::Bind(
                &PassFileInfoToUIThread,
                base::Bind(&FileSystemRetainEntryFunction::RetainFileEntry,
                           this, entry_id, path))));
    return true;
  }

  saved_files_service->EnqueueFileEntry(extension_->id(), entry_id);
  SendResponse(true);
  return true;
}

void FileSystemRetainEntryFunction::RetainFileEntry(
    const std::string& entry_id,
    const base::FilePath& path,
    std::unique_ptr<base::File::Info> file_info) {
  if (!file_info) {
    SendResponse(false);
    return;
  }

  SavedFilesService* saved_files_service = SavedFilesService::Get(GetProfile());
  saved_files_service->RegisterFileEntry(
      extension_->id(), entry_id, path, file_info->is_directory);
  saved_files_service->EnqueueFileEntry(extension_->id(), entry_id);
  SendResponse(true);
}

bool FileSystemIsRestorableFunction::RunSync() {
  std::string entry_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &entry_id));
  SetResult(base::MakeUnique<base::FundamentalValue>(
      SavedFilesService::Get(GetProfile())
          ->IsRegistered(extension_->id(), entry_id)));
  return true;
}

bool FileSystemRestoreEntryFunction::RunAsync() {
  std::string entry_id;
  bool needs_new_entry;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &entry_id));
  EXTENSION_FUNCTION_VALIDATE(args_->GetBoolean(1, &needs_new_entry));
  const SavedFileEntry* file_entry = SavedFilesService::Get(
      GetProfile())->GetFileEntry(extension_->id(), entry_id);
  if (!file_entry) {
    error_ = kUnknownIdError;
    return false;
  }

  SavedFilesService::Get(GetProfile())
      ->EnqueueFileEntry(extension_->id(), entry_id);

  // Only create a new file entry if the renderer requests one.
  // |needs_new_entry| will be false if the renderer already has an Entry for
  // |entry_id|.
  if (needs_new_entry) {
    is_directory_ = file_entry->is_directory;
    std::unique_ptr<base::DictionaryValue> result = CreateResult();
    AddEntryToResult(file_entry->path, file_entry->id, result.get());
    SetResult(std::move(result));
  }
  SendResponse(true);
  return true;
}

bool FileSystemObserveDirectoryFunction::RunSync() {
  NOTIMPLEMENTED();
  error_ = kUnknownIdError;
  return false;
}

bool FileSystemUnobserveEntryFunction::RunSync() {
  NOTIMPLEMENTED();
  error_ = kUnknownIdError;
  return false;
}

bool FileSystemGetObservedEntriesFunction::RunSync() {
  NOTIMPLEMENTED();
  error_ = kUnknownIdError;
  return false;
}

#if !defined(OS_CHROMEOS)
ExtensionFunction::ResponseAction FileSystemRequestFileSystemFunction::Run() {
  using api::file_system::RequestFileSystem::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  NOTIMPLEMENTED();
  return RespondNow(Error(kNotSupportedOnCurrentPlatformError));
}

ExtensionFunction::ResponseAction FileSystemGetVolumeListFunction::Run() {
  NOTIMPLEMENTED();
  return RespondNow(Error(kNotSupportedOnCurrentPlatformError));
}
#else

FileSystemRequestFileSystemFunction::FileSystemRequestFileSystemFunction()
    : chrome_details_(this) {
}

FileSystemRequestFileSystemFunction::~FileSystemRequestFileSystemFunction() {
}

ExtensionFunction::ResponseAction FileSystemRequestFileSystemFunction::Run() {
  using api::file_system::RequestFileSystem::Params;
  const std::unique_ptr<Params> params(Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  // Only kiosk apps in kiosk sessions can use this API.
  // Additionally it is enabled for whitelisted component extensions and apps.
  file_system_api::ConsentProviderDelegate consent_provider_delegate(
      chrome_details_.GetProfile(), render_frame_host());
  file_system_api::ConsentProvider consent_provider(&consent_provider_delegate);

  if (!consent_provider.IsGrantable(*extension()))
    return RespondNow(Error(kNotSupportedOnNonKioskSessionError));

  using file_manager::VolumeManager;
  using file_manager::Volume;
  VolumeManager* const volume_manager =
      VolumeManager::Get(chrome_details_.GetProfile());
  DCHECK(volume_manager);

  const bool writable =
      params->options.writable.get() && *params->options.writable.get();
  if (writable &&
      !app_file_handler_util::HasFileSystemWritePermission(extension_.get())) {
    return RespondNow(Error(kRequiresFileSystemWriteError));
  }

  base::WeakPtr<file_manager::Volume> volume =
      volume_manager->FindVolumeById(params->options.volume_id);
  if (!volume.get())
    return RespondNow(Error(kVolumeNotFoundError));

  const GURL site =
      util::GetSiteForExtensionId(extension_id(), chrome_details_.GetProfile());
  scoped_refptr<storage::FileSystemContext> file_system_context =
      content::BrowserContext::GetStoragePartitionForSite(
          chrome_details_.GetProfile(), site)->GetFileSystemContext();
  storage::ExternalFileSystemBackend* const backend =
      file_system_context->external_backend();
  DCHECK(backend);

  base::FilePath virtual_path;
  if (!backend->GetVirtualPath(volume->mount_path(), &virtual_path))
    return RespondNow(Error(kSecurityError));

  if (writable && (volume->is_read_only()))
    return RespondNow(Error(kSecurityError));

  consent_provider.RequestConsent(
      *extension(), volume, writable,
      base::Bind(&FileSystemRequestFileSystemFunction::OnConsentReceived, this,
                 volume, writable));
  return RespondLater();
}

void FileSystemRequestFileSystemFunction::OnConsentReceived(
    const base::WeakPtr<file_manager::Volume>& volume,
    bool writable,
    ConsentProvider::Consent result) {
  using file_manager::VolumeManager;
  using file_manager::Volume;

  switch (result) {
    case ConsentProvider::CONSENT_REJECTED:
      SetError(kSecurityError);
      SendResponse(false);
      return;

    case ConsentProvider::CONSENT_IMPOSSIBLE:
      SetError(kConsentImpossible);
      SendResponse(false);
      return;

    case ConsentProvider::CONSENT_GRANTED:
      break;
  }

  if (!volume.get()) {
    SetError(kVolumeNotFoundError);
    SendResponse(false);
    return;
  }

  const GURL site =
      util::GetSiteForExtensionId(extension_id(), chrome_details_.GetProfile());
  scoped_refptr<storage::FileSystemContext> file_system_context =
      content::BrowserContext::GetStoragePartitionForSite(
          chrome_details_.GetProfile(), site)->GetFileSystemContext();
  storage::ExternalFileSystemBackend* const backend =
      file_system_context->external_backend();
  DCHECK(backend);

  base::FilePath virtual_path;
  if (!backend->GetVirtualPath(volume->mount_path(), &virtual_path)) {
    SetError(kSecurityError);
    SendResponse(false);
    return;
  }

  storage::IsolatedContext* const isolated_context =
      storage::IsolatedContext::GetInstance();
  DCHECK(isolated_context);

  const storage::FileSystemURL original_url =
      file_system_context->CreateCrackedFileSystemURL(
          GURL(std::string(kExtensionScheme) + url::kStandardSchemeSeparator +
               extension_id()),
          storage::kFileSystemTypeExternal, virtual_path);

  // Set a fixed register name, as the automatic one would leak the mount point
  // directory.
  std::string register_name = "fs";
  const std::string file_system_id =
      isolated_context->RegisterFileSystemForPath(
          storage::kFileSystemTypeNativeForPlatformApp,
          std::string() /* file_system_id */, original_url.path(),
          &register_name);
  if (file_system_id.empty()) {
    SetError(kSecurityError);
    SendResponse(false);
    return;
  }

  backend->GrantFileAccessToExtension(extension_->id(), virtual_path);

  // Grant file permissions to the renderer hosting component.
  content::ChildProcessSecurityPolicy* policy =
      content::ChildProcessSecurityPolicy::GetInstance();
  DCHECK(policy);

  // Read-only permisisons.
  policy->GrantReadFile(render_frame_host()->GetProcess()->GetID(),
                        volume->mount_path());
  policy->GrantReadFileSystem(render_frame_host()->GetProcess()->GetID(),
                              file_system_id);

  // Additional write permissions.
  if (writable) {
    policy->GrantCreateReadWriteFile(render_frame_host()->GetProcess()->GetID(),
                                     volume->mount_path());
    policy->GrantCopyInto(render_frame_host()->GetProcess()->GetID(),
                          volume->mount_path());
    policy->GrantWriteFileSystem(render_frame_host()->GetProcess()->GetID(),
                                 file_system_id);
    policy->GrantDeleteFromFileSystem(
        render_frame_host()->GetProcess()->GetID(), file_system_id);
    policy->GrantCreateFileForFileSystem(
        render_frame_host()->GetProcess()->GetID(), file_system_id);
  }

  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetString("file_system_id", file_system_id);
  dict->SetString("file_system_path", register_name);

  SetResult(std::move(dict));
  SendResponse(true);
}

FileSystemGetVolumeListFunction::FileSystemGetVolumeListFunction()
    : chrome_details_(this) {
}

FileSystemGetVolumeListFunction::~FileSystemGetVolumeListFunction() {
}

ExtensionFunction::ResponseAction FileSystemGetVolumeListFunction::Run() {
  // Only kiosk apps in kiosk sessions can use this API.
  // Additionally it is enabled for whitelisted component extensions and apps.
  file_system_api::ConsentProviderDelegate consent_provider_delegate(
      chrome_details_.GetProfile(), render_frame_host());
  file_system_api::ConsentProvider consent_provider(&consent_provider_delegate);

  if (!consent_provider.IsGrantable(*extension()))
    return RespondNow(Error(kNotSupportedOnNonKioskSessionError));
  std::vector<api::file_system::Volume> result_volume_list;
  FillVolumeList(chrome_details_.GetProfile(), &result_volume_list);

  return RespondNow(ArgumentList(
      api::file_system::GetVolumeList::Results::Create(result_volume_list)));
}
#endif

}  // namespace extensions
