// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/developer_private_api.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/api/developer_private/developer_private_mangle.h"
#include "chrome/browser/extensions/api/developer_private/entry_picker.h"
#include "chrome/browser/extensions/api/developer_private/extension_info_generator.h"
#include "chrome/browser/extensions/api/developer_private/show_permissions_dialog_helper.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/api/file_handlers/app_file_handler_util.h"
#include "chrome/browser/extensions/devtools_util.h"
#include "chrome/browser/extensions/extension_commands_global_registry.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/extensions/shared_module_service.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/extensions/webstore_reinstaller.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/apps/app_info_dialog.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/api/developer_private.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/error_map.h"
#include "extensions/browser/extension_error.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/file_highlighter.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/warning_service.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "storage/browser/fileapi/external_mount_points.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_operation.h"
#include "storage/browser/fileapi/file_system_operation_runner.h"
#include "storage/browser/fileapi/isolated_context.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace developer = api::developer_private;

namespace {

const char kNoSuchExtensionError[] = "No such extension.";
const char kCannotModifyPolicyExtensionError[] =
    "Cannot modify the extension by policy.";
const char kRequiresUserGestureError[] =
    "This action requires a user gesture.";
const char kCouldNotShowSelectFileDialogError[] =
    "Could not show a file chooser.";
const char kFileSelectionCanceled[] =
    "File selection was canceled.";
const char kNoSuchRendererError[] = "No such renderer.";
const char kInvalidPathError[] = "Invalid path.";
const char kManifestKeyIsRequiredError[] =
    "The 'manifestKey' argument is required for manifest files.";
const char kCouldNotFindWebContentsError[] =
    "Could not find a valid web contents.";
const char kCannotUpdateSupervisedProfileSettingsError[] =
    "Cannot change settings for a supervised profile.";
const char kNoOptionsPageForExtensionError[] =
    "Extension does not have an options page.";

const char kUnpackedAppsFolder[] = "apps_target";
const char kManifestFile[] = "manifest.json";

ExtensionService* GetExtensionService(content::BrowserContext* context) {
  return ExtensionSystem::Get(context)->extension_service();
}

std::string ReadFileToString(const base::FilePath& path) {
  std::string data;
  ignore_result(base::ReadFileToString(path, &data));
  return data;
}

bool UserCanModifyExtensionConfiguration(
    const Extension* extension,
    content::BrowserContext* browser_context,
    std::string* error) {
  ManagementPolicy* management_policy =
      ExtensionSystem::Get(browser_context)->management_policy();
  if (!management_policy->UserMayModifySettings(extension, nullptr)) {
    LOG(ERROR) << "Attempt to change settings of an extension that is "
               << "non-usermanagable was made. Extension id : "
               << extension->id();
    *error = kCannotModifyPolicyExtensionError;
    return false;
  }

  return true;
}

// Runs the install verifier for all extensions that are enabled, disabled, or
// terminated.
void PerformVerificationCheck(content::BrowserContext* context) {
  std::unique_ptr<ExtensionSet> extensions =
      ExtensionRegistry::Get(context)->GenerateInstalledExtensionsSet(
          ExtensionRegistry::ENABLED | ExtensionRegistry::DISABLED |
          ExtensionRegistry::TERMINATED);
  ExtensionPrefs* prefs = ExtensionPrefs::Get(context);
  bool should_do_verification_check = false;
  for (const scoped_refptr<const Extension>& extension : *extensions) {
    if (ui_util::ShouldDisplayInExtensionSettings(extension.get(), context) &&
        ((prefs->GetDisableReasons(extension->id()) &
             Extension::DISABLE_NOT_VERIFIED) != 0)) {
      should_do_verification_check = true;
      break;
    }
  }

  UMA_HISTOGRAM_BOOLEAN("ExtensionSettings.ShouldDoVerificationCheck",
                        should_do_verification_check);
  if (should_do_verification_check)
    InstallVerifier::Get(context)->VerifyAllExtensions();
}

std::unique_ptr<developer::ProfileInfo> CreateProfileInfo(Profile* profile) {
  std::unique_ptr<developer::ProfileInfo> info(new developer::ProfileInfo());
  info->is_supervised = profile->IsSupervised();
  PrefService* prefs = profile->GetPrefs();
  info->is_incognito_available =
      IncognitoModePrefs::GetAvailability(prefs) !=
          IncognitoModePrefs::DISABLED;
  info->in_developer_mode =
      !info->is_supervised &&
      prefs->GetBoolean(prefs::kExtensionsUIDeveloperMode);
  info->app_info_dialog_enabled = CanShowAppInfoDialog();
  info->can_load_unpacked =
      !ExtensionManagementFactory::GetForBrowserContext(profile)
          ->BlacklistedByDefault();
  return info;
}

}  // namespace

namespace ChoosePath = api::developer_private::ChoosePath;
namespace GetItemsInfo = api::developer_private::GetItemsInfo;
namespace PackDirectory = api::developer_private::PackDirectory;
namespace Reload = api::developer_private::Reload;

static base::LazyInstance<BrowserContextKeyedAPIFactory<DeveloperPrivateAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<DeveloperPrivateAPI>*
DeveloperPrivateAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

// static
DeveloperPrivateAPI* DeveloperPrivateAPI::Get(
    content::BrowserContext* context) {
  return GetFactoryInstance()->Get(context);
}

DeveloperPrivateAPI::DeveloperPrivateAPI(content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)) {
  RegisterNotifications();
}

DeveloperPrivateEventRouter::DeveloperPrivateEventRouter(Profile* profile)
    : extension_registry_observer_(this),
      error_console_observer_(this),
      process_manager_observer_(this),
      app_window_registry_observer_(this),
      extension_action_api_observer_(this),
      warning_service_observer_(this),
      extension_prefs_observer_(this),
      extension_management_observer_(this),
      command_service_observer_(this),
      profile_(profile),
      event_router_(EventRouter::Get(profile_)),
      weak_factory_(this) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(profile_));
  error_console_observer_.Add(ErrorConsole::Get(profile));
  process_manager_observer_.Add(ProcessManager::Get(profile));
  app_window_registry_observer_.Add(AppWindowRegistry::Get(profile));
  extension_action_api_observer_.Add(ExtensionActionAPI::Get(profile));
  warning_service_observer_.Add(WarningService::Get(profile));
  extension_prefs_observer_.Add(ExtensionPrefs::Get(profile));
  extension_management_observer_.Add(
      ExtensionManagementFactory::GetForBrowserContext(profile));
  command_service_observer_.Add(CommandService::Get(profile));
  pref_change_registrar_.Init(profile->GetPrefs());
  // The unretained is safe, since the PrefChangeRegistrar unregisters the
  // callback on destruction.
  pref_change_registrar_.Add(
      prefs::kExtensionsUIDeveloperMode,
      base::Bind(&DeveloperPrivateEventRouter::OnProfilePrefChanged,
                 base::Unretained(this)));
}

DeveloperPrivateEventRouter::~DeveloperPrivateEventRouter() {
}

void DeveloperPrivateEventRouter::AddExtensionId(
    const std::string& extension_id) {
  extension_ids_.insert(extension_id);
}

void DeveloperPrivateEventRouter::RemoveExtensionId(
    const std::string& extension_id) {
  extension_ids_.erase(extension_id);
}

void DeveloperPrivateEventRouter::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  DCHECK(profile_->IsSameProfile(Profile::FromBrowserContext(browser_context)));
  BroadcastItemStateChanged(developer::EVENT_TYPE_LOADED, extension->id());
}

void DeveloperPrivateEventRouter::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  DCHECK(profile_->IsSameProfile(Profile::FromBrowserContext(browser_context)));
  BroadcastItemStateChanged(developer::EVENT_TYPE_UNLOADED, extension->id());
}

void DeveloperPrivateEventRouter::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update) {
  DCHECK(profile_->IsSameProfile(Profile::FromBrowserContext(browser_context)));
  BroadcastItemStateChanged(developer::EVENT_TYPE_INSTALLED, extension->id());
}

void DeveloperPrivateEventRouter::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  DCHECK(profile_->IsSameProfile(Profile::FromBrowserContext(browser_context)));
  BroadcastItemStateChanged(developer::EVENT_TYPE_UNINSTALLED, extension->id());
}

void DeveloperPrivateEventRouter::OnErrorAdded(const ExtensionError* error) {
  // We don't want to handle errors thrown by extensions subscribed to these
  // events (currently only the Apps Developer Tool), because doing so risks
  // entering a loop.
  if (extension_ids_.count(error->extension_id()))
    return;

  BroadcastItemStateChanged(developer::EVENT_TYPE_ERROR_ADDED,
                            error->extension_id());
}

void DeveloperPrivateEventRouter::OnErrorsRemoved(
    const std::set<std::string>& removed_ids) {
  for (const std::string& id : removed_ids) {
    if (!extension_ids_.count(id))
      BroadcastItemStateChanged(developer::EVENT_TYPE_ERRORS_REMOVED, id);
  }
}

void DeveloperPrivateEventRouter::OnExtensionFrameRegistered(
    const std::string& extension_id,
    content::RenderFrameHost* render_frame_host) {
  BroadcastItemStateChanged(developer::EVENT_TYPE_VIEW_REGISTERED,
                            extension_id);
}

void DeveloperPrivateEventRouter::OnExtensionFrameUnregistered(
    const std::string& extension_id,
    content::RenderFrameHost* render_frame_host) {
  BroadcastItemStateChanged(developer::EVENT_TYPE_VIEW_UNREGISTERED,
                            extension_id);
}

void DeveloperPrivateEventRouter::OnAppWindowAdded(AppWindow* window) {
  BroadcastItemStateChanged(developer::EVENT_TYPE_VIEW_REGISTERED,
                            window->extension_id());
}

void DeveloperPrivateEventRouter::OnAppWindowRemoved(AppWindow* window) {
  BroadcastItemStateChanged(developer::EVENT_TYPE_VIEW_UNREGISTERED,
                            window->extension_id());
}

void DeveloperPrivateEventRouter::OnExtensionCommandAdded(
    const std::string& extension_id,
    const Command& added_command) {
  BroadcastItemStateChanged(developer::EVENT_TYPE_PREFS_CHANGED,
                            extension_id);
}

void DeveloperPrivateEventRouter::OnExtensionCommandRemoved(
    const std::string& extension_id,
    const Command& removed_command) {
  BroadcastItemStateChanged(developer::EVENT_TYPE_PREFS_CHANGED,
                            extension_id);
}

void DeveloperPrivateEventRouter::OnExtensionActionVisibilityChanged(
    const std::string& extension_id,
    bool is_now_visible) {
  BroadcastItemStateChanged(developer::EVENT_TYPE_PREFS_CHANGED, extension_id);
}

void DeveloperPrivateEventRouter::OnExtensionDisableReasonsChanged(
    const std::string& extension_id, int disable_reasons) {
  BroadcastItemStateChanged(developer::EVENT_TYPE_PREFS_CHANGED, extension_id);
}

void DeveloperPrivateEventRouter::OnExtensionManagementSettingsChanged() {
  std::unique_ptr<base::ListValue> args(new base::ListValue());
  args->Append(CreateProfileInfo(profile_)->ToValue());
  std::unique_ptr<Event> event(
      new Event(events::DEVELOPER_PRIVATE_ON_PROFILE_STATE_CHANGED,
                developer::OnProfileStateChanged::kEventName, std::move(args)));
  event_router_->BroadcastEvent(std::move(event));
}

void DeveloperPrivateEventRouter::ExtensionWarningsChanged(
    const ExtensionIdSet& affected_extensions) {
  for (const ExtensionId& id : affected_extensions)
    BroadcastItemStateChanged(developer::EVENT_TYPE_WARNINGS_CHANGED, id);
}

void DeveloperPrivateEventRouter::OnProfilePrefChanged() {
  std::unique_ptr<base::ListValue> args(new base::ListValue());
  args->Append(CreateProfileInfo(profile_)->ToValue());
  std::unique_ptr<Event> event(
      new Event(events::DEVELOPER_PRIVATE_ON_PROFILE_STATE_CHANGED,
                developer::OnProfileStateChanged::kEventName, std::move(args)));
  event_router_->BroadcastEvent(std::move(event));
}

void DeveloperPrivateEventRouter::BroadcastItemStateChanged(
    developer::EventType event_type,
    const std::string& extension_id) {
  std::unique_ptr<ExtensionInfoGenerator> info_generator(
      new ExtensionInfoGenerator(profile_));
  ExtensionInfoGenerator* info_generator_weak = info_generator.get();
  info_generator_weak->CreateExtensionInfo(
      extension_id,
      base::Bind(&DeveloperPrivateEventRouter::BroadcastItemStateChangedHelper,
                 weak_factory_.GetWeakPtr(), event_type, extension_id,
                 base::Passed(std::move(info_generator))));
}

void DeveloperPrivateEventRouter::BroadcastItemStateChangedHelper(
    developer::EventType event_type,
    const std::string& extension_id,
    std::unique_ptr<ExtensionInfoGenerator> info_generator,
    ExtensionInfoGenerator::ExtensionInfoList infos) {
  DCHECK_LE(infos.size(), 1u);

  developer::EventData event_data;
  event_data.event_type = event_type;
  event_data.item_id = extension_id;
  if (!infos.empty()) {
    event_data.extension_info.reset(
        new developer::ExtensionInfo(std::move(infos[0])));
  }

  std::unique_ptr<base::ListValue> args(new base::ListValue());
  args->Append(event_data.ToValue());
  std::unique_ptr<Event> event(
      new Event(events::DEVELOPER_PRIVATE_ON_ITEM_STATE_CHANGED,
                developer::OnItemStateChanged::kEventName, std::move(args)));
  event_router_->BroadcastEvent(std::move(event));
}

void DeveloperPrivateAPI::SetLastUnpackedDirectory(const base::FilePath& path) {
  last_unpacked_directory_ = path;
}

void DeveloperPrivateAPI::RegisterNotifications() {
  EventRouter::Get(profile_)->RegisterObserver(
      this, developer::OnItemStateChanged::kEventName);
}

DeveloperPrivateAPI::~DeveloperPrivateAPI() {}

void DeveloperPrivateAPI::Shutdown() {}

void DeveloperPrivateAPI::OnListenerAdded(
    const EventListenerInfo& details) {
  if (!developer_private_event_router_) {
    developer_private_event_router_.reset(
        new DeveloperPrivateEventRouter(profile_));
  }

  developer_private_event_router_->AddExtensionId(details.extension_id);
}

void DeveloperPrivateAPI::OnListenerRemoved(
    const EventListenerInfo& details) {
  if (!EventRouter::Get(profile_)->HasEventListener(
          developer::OnItemStateChanged::kEventName)) {
    developer_private_event_router_.reset(NULL);
  } else {
    developer_private_event_router_->RemoveExtensionId(details.extension_id);
  }
}

namespace api {

DeveloperPrivateAPIFunction::~DeveloperPrivateAPIFunction() {
}

const Extension* DeveloperPrivateAPIFunction::GetExtensionById(
    const std::string& id) {
  return ExtensionRegistry::Get(browser_context())->GetExtensionById(
      id, ExtensionRegistry::EVERYTHING);
}

const Extension* DeveloperPrivateAPIFunction::GetEnabledExtensionById(
    const std::string& id) {
  return ExtensionRegistry::Get(browser_context())->enabled_extensions().
      GetByID(id);
}

DeveloperPrivateAutoUpdateFunction::~DeveloperPrivateAutoUpdateFunction() {}

ExtensionFunction::ResponseAction DeveloperPrivateAutoUpdateFunction::Run() {
  ExtensionUpdater* updater =
      ExtensionSystem::Get(browser_context())->extension_service()->updater();
  if (updater) {
    ExtensionUpdater::CheckParams params;
    params.install_immediately = true;
    updater->CheckNow(params);
  }
  return RespondNow(NoArguments());
}

DeveloperPrivateGetExtensionsInfoFunction::
DeveloperPrivateGetExtensionsInfoFunction() {
}

DeveloperPrivateGetExtensionsInfoFunction::
~DeveloperPrivateGetExtensionsInfoFunction() {
}

ExtensionFunction::ResponseAction
DeveloperPrivateGetExtensionsInfoFunction::Run() {
  std::unique_ptr<developer::GetExtensionsInfo::Params> params(
      developer::GetExtensionsInfo::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  bool include_disabled = true;
  bool include_terminated = true;
  if (params->options) {
    if (params->options->include_disabled)
      include_disabled = *params->options->include_disabled;
    if (params->options->include_terminated)
      include_terminated = *params->options->include_terminated;
  }

  info_generator_.reset(new ExtensionInfoGenerator(browser_context()));
  info_generator_->CreateExtensionsInfo(
      include_disabled,
      include_terminated,
      base::Bind(&DeveloperPrivateGetExtensionsInfoFunction::OnInfosGenerated,
                 this /* refcounted */));

  return RespondLater();
}

void DeveloperPrivateGetExtensionsInfoFunction::OnInfosGenerated(
    ExtensionInfoGenerator::ExtensionInfoList list) {
  Respond(ArgumentList(developer::GetExtensionsInfo::Results::Create(list)));
}

DeveloperPrivateGetExtensionInfoFunction::
DeveloperPrivateGetExtensionInfoFunction() {
}

DeveloperPrivateGetExtensionInfoFunction::
~DeveloperPrivateGetExtensionInfoFunction() {
}

ExtensionFunction::ResponseAction
DeveloperPrivateGetExtensionInfoFunction::Run() {
  std::unique_ptr<developer::GetExtensionInfo::Params> params(
      developer::GetExtensionInfo::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  info_generator_.reset(new ExtensionInfoGenerator(browser_context()));
  info_generator_->CreateExtensionInfo(
      params->id,
      base::Bind(&DeveloperPrivateGetExtensionInfoFunction::OnInfosGenerated,
                 this /* refcounted */));

  return RespondLater();
}

void DeveloperPrivateGetExtensionInfoFunction::OnInfosGenerated(
    ExtensionInfoGenerator::ExtensionInfoList list) {
  DCHECK_LE(1u, list.size());
  Respond(list.empty() ? Error(kNoSuchExtensionError)
                       : OneArgument(list[0].ToValue()));
}

DeveloperPrivateGetItemsInfoFunction::DeveloperPrivateGetItemsInfoFunction() {}
DeveloperPrivateGetItemsInfoFunction::~DeveloperPrivateGetItemsInfoFunction() {}

ExtensionFunction::ResponseAction DeveloperPrivateGetItemsInfoFunction::Run() {
  std::unique_ptr<developer::GetItemsInfo::Params> params(
      developer::GetItemsInfo::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  info_generator_.reset(new ExtensionInfoGenerator(browser_context()));
  info_generator_->CreateExtensionsInfo(
      params->include_disabled,
      params->include_terminated,
      base::Bind(&DeveloperPrivateGetItemsInfoFunction::OnInfosGenerated,
                 this /* refcounted */));

  return RespondLater();
}

void DeveloperPrivateGetItemsInfoFunction::OnInfosGenerated(
    ExtensionInfoGenerator::ExtensionInfoList list) {
  std::vector<developer::ItemInfo> item_list;
  for (const developer::ExtensionInfo& info : list)
    item_list.push_back(developer_private_mangle::MangleExtensionInfo(info));

  Respond(ArgumentList(developer::GetItemsInfo::Results::Create(item_list)));
}

DeveloperPrivateGetProfileConfigurationFunction::
~DeveloperPrivateGetProfileConfigurationFunction() {
}

ExtensionFunction::ResponseAction
DeveloperPrivateGetProfileConfigurationFunction::Run() {
  std::unique_ptr<developer::ProfileInfo> info =
      CreateProfileInfo(GetProfile());

  // If this is called from the chrome://extensions page, we use this as a
  // heuristic that it's a good time to verify installs. We do this on startup,
  // but there's a chance that it failed erroneously, so it's good to double-
  // check.
  if (source_context_type() == Feature::WEBUI_CONTEXT)
    PerformVerificationCheck(browser_context());

  return RespondNow(OneArgument(info->ToValue()));
}

DeveloperPrivateUpdateProfileConfigurationFunction::
~DeveloperPrivateUpdateProfileConfigurationFunction() {
}

ExtensionFunction::ResponseAction
DeveloperPrivateUpdateProfileConfigurationFunction::Run() {
  std::unique_ptr<developer::UpdateProfileConfiguration::Params> params(
      developer::UpdateProfileConfiguration::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const developer::ProfileConfigurationUpdate& update = params->update;
  PrefService* prefs = GetProfile()->GetPrefs();
  if (update.in_developer_mode) {
    if (GetProfile()->IsSupervised())
      return RespondNow(Error(kCannotUpdateSupervisedProfileSettingsError));
    prefs->SetBoolean(prefs::kExtensionsUIDeveloperMode,
                      *update.in_developer_mode);
  }

  return RespondNow(NoArguments());
}

DeveloperPrivateUpdateExtensionConfigurationFunction::
~DeveloperPrivateUpdateExtensionConfigurationFunction() {}

ExtensionFunction::ResponseAction
DeveloperPrivateUpdateExtensionConfigurationFunction::Run() {
  std::unique_ptr<developer::UpdateExtensionConfiguration::Params> params(
      developer::UpdateExtensionConfiguration::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const developer::ExtensionConfigurationUpdate& update = params->update;

  const Extension* extension = GetExtensionById(update.extension_id);
  if (!extension)
    return RespondNow(Error(kNoSuchExtensionError));
  if (!user_gesture())
    return RespondNow(Error(kRequiresUserGestureError));

  if (update.file_access) {
    std::string error;
    if (!UserCanModifyExtensionConfiguration(extension,
                                             browser_context(),
                                             &error)) {
      return RespondNow(Error(error));
    }
    util::SetAllowFileAccess(
        extension->id(), browser_context(), *update.file_access);
  }
  if (update.incognito_access) {
    util::SetIsIncognitoEnabled(
        extension->id(), browser_context(), *update.incognito_access);
  }
  if (update.error_collection) {
    ErrorConsole::Get(browser_context())->SetReportingAllForExtension(
        extension->id(), *update.error_collection);
  }
  if (update.run_on_all_urls) {
    util::SetAllowedScriptingOnAllUrls(
        extension->id(), browser_context(), *update.run_on_all_urls);
  }
  if (update.show_action_button) {
    ExtensionActionAPI::Get(browser_context())->SetBrowserActionVisibility(
        extension->id(),
        *update.show_action_button);
  }

  return RespondNow(NoArguments());
}

DeveloperPrivateReloadFunction::~DeveloperPrivateReloadFunction() {}

ExtensionFunction::ResponseAction DeveloperPrivateReloadFunction::Run() {
  std::unique_ptr<Reload::Params> params(Reload::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const Extension* extension = GetExtensionById(params->extension_id);
  if (!extension)
    return RespondNow(Error(kNoSuchExtensionError));

  bool fail_quietly = params->options &&
                      params->options->fail_quietly &&
                      *params->options->fail_quietly;

  ExtensionService* service = GetExtensionService(browser_context());
  if (fail_quietly)
    service->ReloadExtensionWithQuietFailure(params->extension_id);
  else
    service->ReloadExtension(params->extension_id);

  // TODO(devlin): We shouldn't return until the extension has finished trying
  // to reload (and then we could also return the error).
  return RespondNow(NoArguments());
}

DeveloperPrivateShowPermissionsDialogFunction::
DeveloperPrivateShowPermissionsDialogFunction() {}

DeveloperPrivateShowPermissionsDialogFunction::
~DeveloperPrivateShowPermissionsDialogFunction() {}

ExtensionFunction::ResponseAction
DeveloperPrivateShowPermissionsDialogFunction::Run() {
  std::unique_ptr<developer::ShowPermissionsDialog::Params> params(
      developer::ShowPermissionsDialog::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const Extension* target_extension = GetExtensionById(params->extension_id);
  if (!target_extension)
    return RespondNow(Error(kNoSuchExtensionError));

  content::WebContents* web_contents = GetSenderWebContents();
  if (!web_contents)
    return RespondNow(Error(kCouldNotFindWebContentsError));

  ShowPermissionsDialogHelper::Show(
      browser_context(),
      web_contents,
      target_extension,
      source_context_type() == Feature::WEBUI_CONTEXT,
      base::Bind(&DeveloperPrivateShowPermissionsDialogFunction::Finish, this));
  return RespondLater();
}

void DeveloperPrivateShowPermissionsDialogFunction::Finish() {
  Respond(NoArguments());
}

DeveloperPrivateLoadUnpackedFunction::DeveloperPrivateLoadUnpackedFunction()
    : fail_quietly_(false) {
}

ExtensionFunction::ResponseAction DeveloperPrivateLoadUnpackedFunction::Run() {
  std::unique_ptr<developer::LoadUnpacked::Params> params(
      developer::LoadUnpacked::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  if (!ShowPicker(
           ui::SelectFileDialog::SELECT_FOLDER,
           l10n_util::GetStringUTF16(IDS_EXTENSION_LOAD_FROM_DIRECTORY),
           ui::SelectFileDialog::FileTypeInfo(),
           0 /* file_type_index */)) {
    return RespondNow(Error(kCouldNotShowSelectFileDialogError));
  }

  fail_quietly_ = params->options &&
                  params->options->fail_quietly &&
                  *params->options->fail_quietly;

  AddRef();  // Balanced in FileSelected / FileSelectionCanceled.
  return RespondLater();
}

void DeveloperPrivateLoadUnpackedFunction::FileSelected(
    const base::FilePath& path) {
  scoped_refptr<UnpackedInstaller> installer(
      UnpackedInstaller::Create(GetExtensionService(browser_context())));
  installer->set_be_noisy_on_failure(!fail_quietly_);
  installer->set_completion_callback(
      base::Bind(&DeveloperPrivateLoadUnpackedFunction::OnLoadComplete, this));
  installer->Load(path);

  DeveloperPrivateAPI::Get(browser_context())->SetLastUnpackedDirectory(path);

  Release();  // Balanced in Run().
}

void DeveloperPrivateLoadUnpackedFunction::FileSelectionCanceled() {
  // This isn't really an error, but we should keep it like this for
  // backward compatability.
  Respond(Error(kFileSelectionCanceled));
  Release();  // Balanced in Run().
}

void DeveloperPrivateLoadUnpackedFunction::OnLoadComplete(
    const Extension* extension,
    const base::FilePath& file_path,
    const std::string& error) {
  Respond(extension ? NoArguments() : Error(error));
}

bool DeveloperPrivateChooseEntryFunction::ShowPicker(
    ui::SelectFileDialog::Type picker_type,
    const base::string16& select_title,
    const ui::SelectFileDialog::FileTypeInfo& info,
    int file_type_index) {
  content::WebContents* web_contents = GetSenderWebContents();
  if (!web_contents)
    return false;

  // The entry picker will hold a reference to this function instance,
  // and subsequent sending of the function response) until the user has
  // selected a file or cancelled the picker. At that point, the picker will
  // delete itself.
  new EntryPicker(this,
                  web_contents,
                  picker_type,
                  DeveloperPrivateAPI::Get(browser_context())->
                      GetLastUnpackedDirectory(),
                  select_title,
                  info,
                  file_type_index);
  return true;
}

DeveloperPrivateChooseEntryFunction::~DeveloperPrivateChooseEntryFunction() {}

void DeveloperPrivatePackDirectoryFunction::OnPackSuccess(
    const base::FilePath& crx_file,
    const base::FilePath& pem_file) {
  developer::PackDirectoryResponse response;
  response.message = base::UTF16ToUTF8(
      PackExtensionJob::StandardSuccessMessage(crx_file, pem_file));
  response.status = developer::PACK_STATUS_SUCCESS;
  Respond(OneArgument(response.ToValue()));
  Release();  // Balanced in Run().
}

void DeveloperPrivatePackDirectoryFunction::OnPackFailure(
    const std::string& error,
    ExtensionCreator::ErrorType error_type) {
  developer::PackDirectoryResponse response;
  response.message = error;
  if (error_type == ExtensionCreator::kCRXExists) {
    response.item_path = item_path_str_;
    response.pem_path = key_path_str_;
    response.override_flags = ExtensionCreator::kOverwriteCRX;
    response.status = developer::PACK_STATUS_WARNING;
  } else {
    response.status = developer::PACK_STATUS_ERROR;
  }
  Respond(OneArgument(response.ToValue()));
  Release();  // Balanced in Run().
}

ExtensionFunction::ResponseAction DeveloperPrivatePackDirectoryFunction::Run() {
  std::unique_ptr<PackDirectory::Params> params(
      PackDirectory::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  int flags = params->flags ? *params->flags : 0;
  item_path_str_ = params->path;
  if (params->private_key_path)
    key_path_str_ = *params->private_key_path;

  base::FilePath root_directory =
      base::FilePath::FromUTF8Unsafe(item_path_str_);
  base::FilePath key_file = base::FilePath::FromUTF8Unsafe(key_path_str_);

  developer::PackDirectoryResponse response;
  if (root_directory.empty()) {
    if (item_path_str_.empty())
      response.message = l10n_util::GetStringUTF8(
          IDS_EXTENSION_PACK_DIALOG_ERROR_ROOT_REQUIRED);
    else
      response.message = l10n_util::GetStringUTF8(
          IDS_EXTENSION_PACK_DIALOG_ERROR_ROOT_INVALID);

    response.status = developer::PACK_STATUS_ERROR;
    return RespondNow(OneArgument(response.ToValue()));
  }

  if (!key_path_str_.empty() && key_file.empty()) {
    response.message = l10n_util::GetStringUTF8(
        IDS_EXTENSION_PACK_DIALOG_ERROR_KEY_INVALID);
    response.status = developer::PACK_STATUS_ERROR;
    return RespondNow(OneArgument(response.ToValue()));
  }

  AddRef();  // Balanced in OnPackSuccess / OnPackFailure.

  // TODO(devlin): Why is PackExtensionJob ref-counted?
  pack_job_ = new PackExtensionJob(this, root_directory, key_file, flags);
  pack_job_->Start();
  return RespondLater();
}

DeveloperPrivatePackDirectoryFunction::DeveloperPrivatePackDirectoryFunction() {
}

DeveloperPrivatePackDirectoryFunction::
~DeveloperPrivatePackDirectoryFunction() {}

DeveloperPrivateLoadUnpackedFunction::~DeveloperPrivateLoadUnpackedFunction() {}

bool DeveloperPrivateLoadDirectoryFunction::RunAsync() {
  // TODO(grv) : add unittests.
  std::string directory_url_str;
  std::string filesystem_name;
  std::string filesystem_path;

  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &filesystem_name));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &filesystem_path));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(2, &directory_url_str));

  context_ = content::BrowserContext::GetStoragePartition(
                 GetProfile(), render_frame_host()->GetSiteInstance())
                 ->GetFileSystemContext();

  // Directory url is non empty only for syncfilesystem.
  if (!directory_url_str.empty()) {
    storage::FileSystemURL directory_url =
        context_->CrackURL(GURL(directory_url_str));
    if (!directory_url.is_valid() ||
        directory_url.type() != storage::kFileSystemTypeSyncable) {
      SetError("DirectoryEntry of unsupported filesystem.");
      return false;
    }
    return LoadByFileSystemAPI(directory_url);
  } else {
    // Check if the DirecotryEntry is the instance of chrome filesystem.
    if (!app_file_handler_util::ValidateFileEntryAndGetPath(
            filesystem_name, filesystem_path,
            render_frame_host()->GetProcess()->GetID(), &project_base_path_,
            &error_)) {
      SetError("DirectoryEntry of unsupported filesystem.");
      return false;
    }

    // Try to load using the FileSystem API backend, in case the filesystem
    // points to a non-native local directory.
    std::string filesystem_id;
    bool cracked =
        storage::CrackIsolatedFileSystemName(filesystem_name, &filesystem_id);
    CHECK(cracked);
    base::FilePath virtual_path =
        storage::IsolatedContext::GetInstance()
            ->CreateVirtualRootPath(filesystem_id)
            .Append(base::FilePath::FromUTF8Unsafe(filesystem_path));
    storage::FileSystemURL directory_url = context_->CreateCrackedFileSystemURL(
        extensions::Extension::GetBaseURLFromExtensionId(extension_id()),
        storage::kFileSystemTypeIsolated,
        virtual_path);

    if (directory_url.is_valid() &&
        directory_url.type() != storage::kFileSystemTypeNativeLocal &&
        directory_url.type() != storage::kFileSystemTypeRestrictedNativeLocal &&
        directory_url.type() != storage::kFileSystemTypeDragged) {
      return LoadByFileSystemAPI(directory_url);
    }

    Load();
  }

  return true;
}

bool DeveloperPrivateLoadDirectoryFunction::LoadByFileSystemAPI(
    const storage::FileSystemURL& directory_url) {
  std::string directory_url_str = directory_url.ToGURL().spec();

  size_t pos = 0;
  // Parse the project directory name from the project url. The project url is
  // expected to have project name as the suffix.
  if ((pos = directory_url_str.rfind("/")) == std::string::npos) {
    SetError("Invalid Directory entry.");
    return false;
  }

  std::string project_name;
  project_name = directory_url_str.substr(pos + 1);
  project_base_url_ = directory_url_str.substr(0, pos + 1);

  base::FilePath project_path(GetProfile()->GetPath());
  project_path = project_path.AppendASCII(kUnpackedAppsFolder);
  project_path = project_path.Append(
      base::FilePath::FromUTF8Unsafe(project_name));

  project_base_path_ = project_path;

  content::BrowserThread::PostTask(content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&DeveloperPrivateLoadDirectoryFunction::
                     ClearExistingDirectoryContent,
                 this,
                 project_base_path_));
  return true;
}

void DeveloperPrivateLoadDirectoryFunction::Load() {
  ExtensionService* service = GetExtensionService(GetProfile());
  UnpackedInstaller::Create(service)->Load(project_base_path_);

  // TODO(grv) : The unpacked installer should fire an event when complete
  // and return the extension_id.
  SetResult(base::MakeUnique<base::StringValue>("-1"));
  SendResponse(true);
}

void DeveloperPrivateLoadDirectoryFunction::ClearExistingDirectoryContent(
    const base::FilePath& project_path) {

  // Clear the project directory before copying new files.
  base::DeleteFile(project_path, true /*recursive*/);

  pending_copy_operations_count_ = 1;

  content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE,
      base::Bind(&DeveloperPrivateLoadDirectoryFunction::
                 ReadDirectoryByFileSystemAPI,
                 this, project_path, project_path.BaseName()));
}

void DeveloperPrivateLoadDirectoryFunction::ReadDirectoryByFileSystemAPI(
    const base::FilePath& project_path,
    const base::FilePath& destination_path) {
  GURL project_url = GURL(project_base_url_ + destination_path.AsUTF8Unsafe());
  storage::FileSystemURL url = context_->CrackURL(project_url);

  context_->operation_runner()->ReadDirectory(
      url, base::Bind(&DeveloperPrivateLoadDirectoryFunction::
                      ReadDirectoryByFileSystemAPICb,
                      this, project_path, destination_path));
}

void DeveloperPrivateLoadDirectoryFunction::ReadDirectoryByFileSystemAPICb(
    const base::FilePath& project_path,
    const base::FilePath& destination_path,
    base::File::Error status,
    const storage::FileSystemOperation::FileEntryList& file_list,
    bool has_more) {
  if (status != base::File::FILE_OK) {
    DLOG(ERROR) << "Error in copying files from sync filesystem.";
    return;
  }

  // We add 1 to the pending copy operations for both files and directories. We
  // release the directory copy operation once all the files under the directory
  // are added for copying. We do that to ensure that pendingCopyOperationsCount
  // does not become zero before all copy operations are finished.
  // In case the directory happens to be executing the last copy operation it
  // will call SendResponse to send the response to the API. The pending copy
  // operations of files are released by the CopyFile function.
  pending_copy_operations_count_ += file_list.size();

  for (size_t i = 0; i < file_list.size(); ++i) {
    if (file_list[i].is_directory) {
      ReadDirectoryByFileSystemAPI(project_path.Append(file_list[i].name),
                                   destination_path.Append(file_list[i].name));
      continue;
    }

    GURL project_url = GURL(project_base_url_ +
        destination_path.Append(file_list[i].name).AsUTF8Unsafe());
    storage::FileSystemURL url = context_->CrackURL(project_url);

    base::FilePath target_path = project_path;
    target_path = target_path.Append(file_list[i].name);

    context_->operation_runner()->CreateSnapshotFile(
        url,
        base::Bind(&DeveloperPrivateLoadDirectoryFunction::SnapshotFileCallback,
            this,
            target_path));
  }

  if (!has_more) {
    // Directory copy operation released here.
    pending_copy_operations_count_--;

    if (!pending_copy_operations_count_) {
      content::BrowserThread::PostTask(
          content::BrowserThread::UI, FROM_HERE,
          base::Bind(&DeveloperPrivateLoadDirectoryFunction::SendResponse,
                     this,
                     success_));
    }
  }
}

void DeveloperPrivateLoadDirectoryFunction::SnapshotFileCallback(
    const base::FilePath& target_path,
    base::File::Error result,
    const base::File::Info& file_info,
    const base::FilePath& src_path,
    const scoped_refptr<storage::ShareableFileReference>& file_ref) {
  if (result != base::File::FILE_OK) {
    SetError("Error in copying files from sync filesystem.");
    success_ = false;
    return;
  }

  content::BrowserThread::PostTask(content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&DeveloperPrivateLoadDirectoryFunction::CopyFile,
                 this,
                 src_path,
                 target_path));
}

void DeveloperPrivateLoadDirectoryFunction::CopyFile(
    const base::FilePath& src_path,
    const base::FilePath& target_path) {
  if (!base::CreateDirectory(target_path.DirName())) {
    SetError("Error in copying files from sync filesystem.");
    success_ = false;
  }

  if (success_)
    base::CopyFile(src_path, target_path);

  CHECK(pending_copy_operations_count_ > 0);
  pending_copy_operations_count_--;

  if (!pending_copy_operations_count_) {
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
        base::Bind(&DeveloperPrivateLoadDirectoryFunction::Load,
                   this));
  }
}

DeveloperPrivateLoadDirectoryFunction::DeveloperPrivateLoadDirectoryFunction()
    : pending_copy_operations_count_(0), success_(true) {}

DeveloperPrivateLoadDirectoryFunction::~DeveloperPrivateLoadDirectoryFunction()
    {}

ExtensionFunction::ResponseAction DeveloperPrivateChoosePathFunction::Run() {
  std::unique_ptr<developer::ChoosePath::Params> params(
      developer::ChoosePath::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  ui::SelectFileDialog::Type type = ui::SelectFileDialog::SELECT_FOLDER;
  ui::SelectFileDialog::FileTypeInfo info;

  if (params->select_type == developer::SELECT_TYPE_FILE)
    type = ui::SelectFileDialog::SELECT_OPEN_FILE;
  base::string16 select_title;

  int file_type_index = 0;
  if (params->file_type == developer::FILE_TYPE_LOAD) {
    select_title = l10n_util::GetStringUTF16(IDS_EXTENSION_LOAD_FROM_DIRECTORY);
  } else if (params->file_type == developer::FILE_TYPE_PEM) {
    select_title = l10n_util::GetStringUTF16(
        IDS_EXTENSION_PACK_DIALOG_SELECT_KEY);
    info.extensions.push_back(std::vector<base::FilePath::StringType>(
        1, FILE_PATH_LITERAL("pem")));
    info.extension_description_overrides.push_back(
        l10n_util::GetStringUTF16(
            IDS_EXTENSION_PACK_DIALOG_KEY_FILE_TYPE_DESCRIPTION));
    info.include_all_files = true;
    file_type_index = 1;
  } else {
    NOTREACHED();
  }

  if (!ShowPicker(
           type,
           select_title,
           info,
           file_type_index)) {
    return RespondNow(Error(kCouldNotShowSelectFileDialogError));
  }

  AddRef();  // Balanced by FileSelected / FileSelectionCanceled.
  return RespondLater();
}

void DeveloperPrivateChoosePathFunction::FileSelected(
    const base::FilePath& path) {
  Respond(OneArgument(
      base::MakeUnique<base::StringValue>(path.LossyDisplayName())));
  Release();
}

void DeveloperPrivateChoosePathFunction::FileSelectionCanceled() {
  // This isn't really an error, but we should keep it like this for
  // backward compatability.
  Respond(Error(kFileSelectionCanceled));
  Release();
}

DeveloperPrivateChoosePathFunction::~DeveloperPrivateChoosePathFunction() {}

bool DeveloperPrivateIsProfileManagedFunction::RunSync() {
  SetResult(
      base::MakeUnique<base::FundamentalValue>(GetProfile()->IsSupervised()));
  return true;
}

DeveloperPrivateIsProfileManagedFunction::
    ~DeveloperPrivateIsProfileManagedFunction() {
}

DeveloperPrivateRequestFileSourceFunction::
    DeveloperPrivateRequestFileSourceFunction() {}

DeveloperPrivateRequestFileSourceFunction::
    ~DeveloperPrivateRequestFileSourceFunction() {}

ExtensionFunction::ResponseAction
DeveloperPrivateRequestFileSourceFunction::Run() {
  params_ = developer::RequestFileSource::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_);

  const developer::RequestFileSourceProperties& properties =
      params_->properties;
  const Extension* extension = GetExtensionById(properties.extension_id);
  if (!extension)
    return RespondNow(Error(kNoSuchExtensionError));

  // Under no circumstances should we ever need to reference a file outside of
  // the extension's directory. If it tries to, abort.
  base::FilePath path_suffix =
      base::FilePath::FromUTF8Unsafe(properties.path_suffix);
  if (path_suffix.empty() || path_suffix.ReferencesParent())
    return RespondNow(Error(kInvalidPathError));

  if (properties.path_suffix == kManifestFile && !properties.manifest_key)
    return RespondNow(Error(kManifestKeyIsRequiredError));

  base::PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&ReadFileToString, extension->path().Append(path_suffix)),
      base::Bind(&DeveloperPrivateRequestFileSourceFunction::Finish, this));

  return RespondLater();
}

void DeveloperPrivateRequestFileSourceFunction::Finish(
    const std::string& file_contents) {
  const developer::RequestFileSourceProperties& properties =
      params_->properties;
  const Extension* extension = GetExtensionById(properties.extension_id);
  if (!extension) {
    Respond(Error(kNoSuchExtensionError));
    return;
  }

  developer::RequestFileSourceResponse response;
  base::FilePath path_suffix =
      base::FilePath::FromUTF8Unsafe(properties.path_suffix);
  base::FilePath path = extension->path().Append(path_suffix);
  response.title = base::StringPrintf("%s: %s",
                                      extension->name().c_str(),
                                      path.BaseName().AsUTF8Unsafe().c_str());
  response.message = properties.message;

  std::unique_ptr<FileHighlighter> highlighter;
  if (properties.path_suffix == kManifestFile) {
    highlighter.reset(new ManifestHighlighter(
        file_contents,
        *properties.manifest_key,
        properties.manifest_specific ?
            *properties.manifest_specific : std::string()));
  } else {
    highlighter.reset(new SourceHighlighter(
        file_contents,
        properties.line_number ? *properties.line_number : 0));
  }

  response.before_highlight = highlighter->GetBeforeFeature();
  response.highlight = highlighter->GetFeature();
  response.after_highlight = highlighter->GetAfterFeature();

  Respond(OneArgument(response.ToValue()));
}

DeveloperPrivateOpenDevToolsFunction::DeveloperPrivateOpenDevToolsFunction() {}
DeveloperPrivateOpenDevToolsFunction::~DeveloperPrivateOpenDevToolsFunction() {}

ExtensionFunction::ResponseAction
DeveloperPrivateOpenDevToolsFunction::Run() {
  std::unique_ptr<developer::OpenDevTools::Params> params(
      developer::OpenDevTools::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  const developer::OpenDevToolsProperties& properties = params->properties;

  if (properties.render_process_id == -1) {
    // This is a lazy background page.
    const Extension* extension = properties.extension_id ?
        GetEnabledExtensionById(*properties.extension_id) : nullptr;
    if (!extension)
      return RespondNow(Error(kNoSuchExtensionError));

    Profile* profile = GetProfile();
    if (properties.incognito && *properties.incognito)
      profile = profile->GetOffTheRecordProfile();

    // Wakes up the background page and opens the inspect window.
    devtools_util::InspectBackgroundPage(extension, profile);
    return RespondNow(NoArguments());
  }

  // NOTE(devlin): Even though the properties use "render_view_id", this
  // actually refers to a render frame.
  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(
      properties.render_process_id, properties.render_view_id);

  content::WebContents* web_contents =
      rfh ? content::WebContents::FromRenderFrameHost(rfh) : nullptr;
  // It's possible that the render frame was closed since we last updated the
  // links. Handle this gracefully.
  if (!web_contents)
    return RespondNow(Error(kNoSuchRendererError));

  // If we include a url, we should inspect it specifically (and not just the
  // render frame).
  if (properties.url) {
    // Line/column numbers are reported in display-friendly 1-based numbers,
    // but are inspected in zero-based numbers.
    // Default to the first line/column.
    DevToolsWindow::OpenDevToolsWindow(
        web_contents,
        DevToolsToggleAction::Reveal(
            base::UTF8ToUTF16(*properties.url),
            properties.line_number ? *properties.line_number - 1 : 0,
            properties.column_number ? *properties.column_number - 1 : 0));
  } else {
    DevToolsWindow::OpenDevToolsWindow(web_contents);
  }

  // Once we open the inspector, we focus on the appropriate tab...
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);

  // ... but some pages (popups and apps) don't have tabs, and some (background
  // pages) don't have an associated browser. For these, the inspector opens in
  // a new window, and our work is done.
  if (!browser || !browser->is_type_tabbed())
    return RespondNow(NoArguments());

  TabStripModel* tab_strip = browser->tab_strip_model();
  tab_strip->ActivateTabAt(tab_strip->GetIndexOfWebContents(web_contents),
                           false);  // Not through direct user gesture.
  return RespondNow(NoArguments());
}

DeveloperPrivateDeleteExtensionErrorsFunction::
~DeveloperPrivateDeleteExtensionErrorsFunction() {}

ExtensionFunction::ResponseAction
DeveloperPrivateDeleteExtensionErrorsFunction::Run() {
  std::unique_ptr<developer::DeleteExtensionErrors::Params> params(
      developer::DeleteExtensionErrors::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  const developer::DeleteExtensionErrorsProperties& properties =
      params->properties;

  ErrorConsole* error_console = ErrorConsole::Get(GetProfile());
  int type = -1;
  if (properties.type != developer::ERROR_TYPE_NONE) {
    type = properties.type == developer::ERROR_TYPE_MANIFEST ?
        ExtensionError::MANIFEST_ERROR : ExtensionError::RUNTIME_ERROR;
  }
  std::set<int> error_ids;
  if (properties.error_ids) {
    error_ids.insert(properties.error_ids->begin(),
                     properties.error_ids->end());
  }
  error_console->RemoveErrors(ErrorMap::Filter(
      properties.extension_id, type, error_ids, false));

  return RespondNow(NoArguments());
}

DeveloperPrivateRepairExtensionFunction::
~DeveloperPrivateRepairExtensionFunction() {}

ExtensionFunction::ResponseAction
DeveloperPrivateRepairExtensionFunction::Run() {
  std::unique_ptr<developer::RepairExtension::Params> params(
      developer::RepairExtension::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  const Extension* extension = GetExtensionById(params->extension_id);
  if (!extension)
    return RespondNow(Error(kNoSuchExtensionError));

  content::WebContents* web_contents = GetSenderWebContents();
  if (!web_contents)
    return RespondNow(Error(kCouldNotFindWebContentsError));

  scoped_refptr<WebstoreReinstaller> reinstaller(new WebstoreReinstaller(
      web_contents,
      params->extension_id,
      base::Bind(&DeveloperPrivateRepairExtensionFunction::OnReinstallComplete,
                 this)));
  reinstaller->BeginReinstall();

  return RespondLater();
}

void DeveloperPrivateRepairExtensionFunction::OnReinstallComplete(
    bool success,
    const std::string& error,
    webstore_install::Result result) {
  Respond(success ? NoArguments() : Error(error));
}

DeveloperPrivateShowOptionsFunction::~DeveloperPrivateShowOptionsFunction() {}

ExtensionFunction::ResponseAction DeveloperPrivateShowOptionsFunction::Run() {
  std::unique_ptr<developer::ShowOptions::Params> params(
      developer::ShowOptions::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  const Extension* extension = GetEnabledExtensionById(params->extension_id);
  if (!extension)
    return RespondNow(Error(kNoSuchExtensionError));

  if (OptionsPageInfo::GetOptionsPage(extension).is_empty())
    return RespondNow(Error(kNoOptionsPageForExtensionError));

  content::WebContents* web_contents = GetSenderWebContents();
  if (!web_contents)
    return RespondNow(Error(kCouldNotFindWebContentsError));

  ExtensionTabUtil::OpenOptionsPage(
      extension,
      chrome::FindBrowserWithWebContents(web_contents));
  return RespondNow(NoArguments());
}

DeveloperPrivateShowPathFunction::~DeveloperPrivateShowPathFunction() {}

ExtensionFunction::ResponseAction DeveloperPrivateShowPathFunction::Run() {
  std::unique_ptr<developer::ShowPath::Params> params(
      developer::ShowPath::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  const Extension* extension = GetExtensionById(params->extension_id);
  if (!extension)
    return RespondNow(Error(kNoSuchExtensionError));

  // We explicitly show manifest.json in order to work around an issue in OSX
  // where opening the directory doesn't focus the Finder.
  platform_util::ShowItemInFolder(GetProfile(),
                                  extension->path().Append(kManifestFilename));
  return RespondNow(NoArguments());
}

DeveloperPrivateSetShortcutHandlingSuspendedFunction::
~DeveloperPrivateSetShortcutHandlingSuspendedFunction() {}

ExtensionFunction::ResponseAction
DeveloperPrivateSetShortcutHandlingSuspendedFunction::Run() {
  std::unique_ptr<developer::SetShortcutHandlingSuspended::Params> params(
      developer::SetShortcutHandlingSuspended::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  ExtensionCommandsGlobalRegistry::Get(GetProfile())->
      SetShortcutHandlingSuspended(params->is_suspended);
  return RespondNow(NoArguments());
}

DeveloperPrivateUpdateExtensionCommandFunction::
~DeveloperPrivateUpdateExtensionCommandFunction() {}

ExtensionFunction::ResponseAction
DeveloperPrivateUpdateExtensionCommandFunction::Run() {
  std::unique_ptr<developer::UpdateExtensionCommand::Params> params(
      developer::UpdateExtensionCommand::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  const developer::ExtensionCommandUpdate& update = params->update;

  CommandService* command_service = CommandService::Get(GetProfile());

  if (update.scope != developer::COMMAND_SCOPE_NONE) {
    command_service->SetScope(update.extension_id, update.command_name,
                              update.scope == developer::COMMAND_SCOPE_GLOBAL);
  }

  if (update.keybinding) {
    command_service->UpdateKeybindingPrefs(
        update.extension_id, update.command_name, *update.keybinding);
  }

  return RespondNow(NoArguments());
}


}  // namespace api

}  // namespace extensions
