// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/management/management_api.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/management/management_api_constants.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/requirements_checker.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/api/management.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/manifest_handlers/offline_enabled_info.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/manifest_url_handlers.h"
#include "extensions/common/permissions/permission_message.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"

using base::IntToString;
using content::BrowserThread;

namespace keys = extension_management_api_constants;

namespace extensions {

namespace management = api::management;

namespace {

typedef std::vector<management::ExtensionInfo> ExtensionInfoList;
typedef std::vector<management::IconInfo> IconInfoList;

enum AutoConfirmForTest { DO_NOT_SKIP = 0, PROCEED, ABORT };

AutoConfirmForTest auto_confirm_for_test = DO_NOT_SKIP;

std::vector<std::string> CreateWarningsList(const Extension* extension) {
  std::vector<std::string> warnings_list;
  for (const PermissionMessage& msg :
       extension->permissions_data()->GetPermissionMessages()) {
    warnings_list.push_back(base::UTF16ToUTF8(msg.message()));
  }

  return warnings_list;
}

std::vector<management::LaunchType> GetAvailableLaunchTypes(
    const Extension& extension,
    const ManagementAPIDelegate* delegate) {
  std::vector<management::LaunchType> launch_type_list;
  if (extension.is_platform_app()) {
    launch_type_list.push_back(management::LAUNCH_TYPE_OPEN_AS_WINDOW);
    return launch_type_list;
  }

  launch_type_list.push_back(management::LAUNCH_TYPE_OPEN_AS_REGULAR_TAB);

  // TODO(dominickn): remove check when hosted apps can open in windows on Mac.
  if (delegate->CanHostedAppsOpenInWindows())
    launch_type_list.push_back(management::LAUNCH_TYPE_OPEN_AS_WINDOW);

  if (!delegate->IsNewBookmarkAppsEnabled()) {
    launch_type_list.push_back(management::LAUNCH_TYPE_OPEN_AS_PINNED_TAB);
    launch_type_list.push_back(management::LAUNCH_TYPE_OPEN_FULL_SCREEN);
  }
  return launch_type_list;
}

management::ExtensionInfo CreateExtensionInfo(
    const Extension& extension,
    content::BrowserContext* context) {
  ExtensionSystem* system = ExtensionSystem::Get(context);
  ExtensionRegistry* registry = ExtensionRegistry::Get(context);
  const ManagementAPIDelegate* delegate =
      ManagementAPI::GetFactoryInstance()->Get(context)->GetDelegate();
  management::ExtensionInfo info;

  info.id = extension.id();
  info.name = extension.name();
  info.short_name = extension.short_name();
  info.enabled = registry->enabled_extensions().Contains(info.id);
  info.offline_enabled = OfflineEnabledInfo::IsOfflineEnabled(&extension);
  info.version = extension.VersionString();
  if (!extension.version_name().empty())
    info.version_name.reset(new std::string(extension.version_name()));
  info.description = extension.description();
  info.options_url = OptionsPageInfo::GetOptionsPage(&extension).spec();
  info.homepage_url.reset(
      new std::string(ManifestURL::GetHomepageURL(&extension).spec()));
  info.may_disable =
      system->management_policy()->UserMayModifySettings(&extension, NULL);
  info.is_app = extension.is_app();
  if (info.is_app) {
    if (extension.is_legacy_packaged_app())
      info.type = management::EXTENSION_TYPE_LEGACY_PACKAGED_APP;
    else if (extension.is_hosted_app())
      info.type = management::EXTENSION_TYPE_HOSTED_APP;
    else
      info.type = management::EXTENSION_TYPE_PACKAGED_APP;
  } else if (extension.is_theme()) {
    info.type = management::EXTENSION_TYPE_THEME;
  } else {
    info.type = management::EXTENSION_TYPE_EXTENSION;
  }

  if (info.enabled) {
    info.disabled_reason = management::EXTENSION_DISABLED_REASON_NONE;
  } else {
    ExtensionPrefs* prefs = ExtensionPrefs::Get(context);
    if (prefs->DidExtensionEscalatePermissions(extension.id())) {
      info.disabled_reason =
          management::EXTENSION_DISABLED_REASON_PERMISSIONS_INCREASE;
    } else {
      info.disabled_reason = management::EXTENSION_DISABLED_REASON_UNKNOWN;
    }
  }

  if (!ManifestURL::GetUpdateURL(&extension).is_empty()) {
    info.update_url.reset(
        new std::string(ManifestURL::GetUpdateURL(&extension).spec()));
  }

  if (extension.is_app()) {
    info.app_launch_url.reset(
        new std::string(delegate->GetFullLaunchURL(&extension).spec()));
  }

  const ExtensionIconSet::IconMap& icons =
      IconsInfo::GetIcons(&extension).map();
  if (!icons.empty()) {
    info.icons.reset(new IconInfoList());
    ExtensionIconSet::IconMap::const_iterator icon_iter;
    for (icon_iter = icons.begin(); icon_iter != icons.end(); ++icon_iter) {
      management::IconInfo icon_info;
      icon_info.size = icon_iter->first;
      GURL url =
          delegate->GetIconURL(&extension, icon_info.size,
                               ExtensionIconSet::MATCH_EXACTLY, false, nullptr);
      icon_info.url = url.spec();
      info.icons->push_back(std::move(icon_info));
    }
  }

  const std::set<std::string> perms =
      extension.permissions_data()->active_permissions().GetAPIsAsStrings();
  if (!perms.empty()) {
    std::set<std::string>::const_iterator perms_iter;
    for (perms_iter = perms.begin(); perms_iter != perms.end(); ++perms_iter)
      info.permissions.push_back(*perms_iter);
  }

  if (!extension.is_hosted_app()) {
    // Skip host permissions for hosted apps.
    const URLPatternSet host_perms =
        extension.permissions_data()->active_permissions().explicit_hosts();
    if (!host_perms.is_empty()) {
      for (URLPatternSet::const_iterator iter = host_perms.begin();
           iter != host_perms.end(); ++iter) {
        info.host_permissions.push_back(iter->GetAsString());
      }
    }
  }

  switch (extension.location()) {
    case Manifest::INTERNAL:
      info.install_type = management::EXTENSION_INSTALL_TYPE_NORMAL;
      break;
    case Manifest::UNPACKED:
    case Manifest::COMMAND_LINE:
      info.install_type = management::EXTENSION_INSTALL_TYPE_DEVELOPMENT;
      break;
    case Manifest::EXTERNAL_PREF:
    case Manifest::EXTERNAL_REGISTRY:
    case Manifest::EXTERNAL_PREF_DOWNLOAD:
      info.install_type = management::EXTENSION_INSTALL_TYPE_SIDELOAD;
      break;
    case Manifest::EXTERNAL_POLICY:
    case Manifest::EXTERNAL_POLICY_DOWNLOAD:
      info.install_type = management::EXTENSION_INSTALL_TYPE_ADMIN;
      break;
    case Manifest::NUM_LOCATIONS:
      NOTREACHED();
    case Manifest::INVALID_LOCATION:
    case Manifest::COMPONENT:
    case Manifest::EXTERNAL_COMPONENT:
      info.install_type = management::EXTENSION_INSTALL_TYPE_OTHER;
      break;
  }

  info.launch_type = management::LAUNCH_TYPE_NONE;
  if (extension.is_app()) {
    LaunchType launch_type;
    if (extension.is_platform_app()) {
      launch_type = LAUNCH_TYPE_WINDOW;
    } else {
      launch_type =
          delegate->GetLaunchType(ExtensionPrefs::Get(context), &extension);
    }

    switch (launch_type) {
      case LAUNCH_TYPE_PINNED:
        info.launch_type = management::LAUNCH_TYPE_OPEN_AS_PINNED_TAB;
        break;
      case LAUNCH_TYPE_REGULAR:
        info.launch_type = management::LAUNCH_TYPE_OPEN_AS_REGULAR_TAB;
        break;
      case LAUNCH_TYPE_FULLSCREEN:
        info.launch_type = management::LAUNCH_TYPE_OPEN_FULL_SCREEN;
        break;
      case LAUNCH_TYPE_WINDOW:
        info.launch_type = management::LAUNCH_TYPE_OPEN_AS_WINDOW;
        break;
      case LAUNCH_TYPE_INVALID:
      case NUM_LAUNCH_TYPES:
        NOTREACHED();
    }

    info.available_launch_types.reset(new std::vector<management::LaunchType>(
        GetAvailableLaunchTypes(extension, delegate)));
  }

  return info;
}

void AddExtensionInfo(const ExtensionSet& extensions,
                      ExtensionInfoList* extension_list,
                      content::BrowserContext* context) {
  for (ExtensionSet::const_iterator iter = extensions.begin();
       iter != extensions.end(); ++iter) {
    const Extension& extension = *iter->get();

    if (extension.ShouldNotBeVisible())
      continue;  // Skip built-in extensions/apps.

    extension_list->push_back(CreateExtensionInfo(extension, context));
  }
}

}  // namespace

bool ManagementGetAllFunction::RunSync() {
  ExtensionInfoList extensions;
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());

  AddExtensionInfo(registry->enabled_extensions(), &extensions,
                   browser_context());
  AddExtensionInfo(registry->disabled_extensions(), &extensions,
                   browser_context());
  AddExtensionInfo(registry->terminated_extensions(), &extensions,
                   browser_context());

  results_ = management::GetAll::Results::Create(extensions);
  return true;
}

bool ManagementGetFunction::RunSync() {
  std::unique_ptr<management::Get::Params> params(
      management::Get::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());

  const Extension* extension =
      registry->GetExtensionById(params->id, ExtensionRegistry::EVERYTHING);
  if (!extension) {
    error_ =
        ErrorUtils::FormatErrorMessage(keys::kNoExtensionError, params->id);
    return false;
  }

  results_ = management::Get::Results::Create(
      CreateExtensionInfo(*extension, browser_context()));

  return true;
}

bool ManagementGetSelfFunction::RunSync() {
  results_ = management::Get::Results::Create(
      CreateExtensionInfo(*extension_, browser_context()));

  return true;
}

bool ManagementGetPermissionWarningsByIdFunction::RunSync() {
  std::unique_ptr<management::GetPermissionWarningsById::Params> params(
      management::GetPermissionWarningsById::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const Extension* extension =
      ExtensionRegistry::Get(browser_context())
          ->GetExtensionById(params->id, ExtensionRegistry::EVERYTHING);
  if (!extension) {
    error_ =
        ErrorUtils::FormatErrorMessage(keys::kNoExtensionError, params->id);
    return false;
  }

  std::vector<std::string> warnings = CreateWarningsList(extension);
  results_ = management::GetPermissionWarningsById::Results::Create(warnings);
  return true;
}

bool ManagementGetPermissionWarningsByManifestFunction::RunAsync() {
  std::unique_ptr<management::GetPermissionWarningsByManifest::Params> params(
      management::GetPermissionWarningsByManifest::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  const ManagementAPIDelegate* delegate = ManagementAPI::GetFactoryInstance()
                                              ->Get(browser_context())
                                              ->GetDelegate();

  if (delegate) {
    delegate->GetPermissionWarningsByManifestFunctionDelegate(
        this, params->manifest_str);

    // Matched with a Release() in OnParseSuccess/Failure().
    AddRef();

    // Response is sent async in OnParseSuccess/Failure().
    return true;
  } else {
    // TODO(lfg) add error string
    OnParseFailure("");
    return false;
  }
}

void ManagementGetPermissionWarningsByManifestFunction::OnParseSuccess(
    std::unique_ptr<base::Value> value) {
  if (!value->IsType(base::Value::TYPE_DICTIONARY)) {
    OnParseFailure(keys::kManifestParseError);
    return;
  }
  const base::DictionaryValue* parsed_manifest =
      static_cast<const base::DictionaryValue*>(value.get());

  scoped_refptr<Extension> extension =
      Extension::Create(base::FilePath(), Manifest::INVALID_LOCATION,
                        *parsed_manifest, Extension::NO_FLAGS, &error_);
  if (!extension) {
    OnParseFailure(keys::kExtensionCreateError);
    return;
  }

  std::vector<std::string> warnings = CreateWarningsList(extension.get());
  results_ =
      management::GetPermissionWarningsByManifest::Results::Create(warnings);
  SendResponse(true);

  // Matched with AddRef() in RunAsync().
  Release();
}

void ManagementGetPermissionWarningsByManifestFunction::OnParseFailure(
    const std::string& error) {
  error_ = error;
  SendResponse(false);

  // Matched with AddRef() in RunAsync().
  Release();
}

bool ManagementLaunchAppFunction::RunSync() {
  std::unique_ptr<management::LaunchApp::Params> params(
      management::LaunchApp::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  const Extension* extension =
      ExtensionRegistry::Get(browser_context())
          ->GetExtensionById(params->id, ExtensionRegistry::EVERYTHING);
  if (!extension) {
    error_ =
        ErrorUtils::FormatErrorMessage(keys::kNoExtensionError, params->id);
    return false;
  }
  if (!extension->is_app()) {
    error_ = ErrorUtils::FormatErrorMessage(keys::kNotAnAppError, params->id);
    return false;
  }

  const ManagementAPIDelegate* delegate = ManagementAPI::GetFactoryInstance()
                                              ->Get(browser_context())
                                              ->GetDelegate();
  return delegate->LaunchAppFunctionDelegate(extension, browser_context());
}

ManagementSetEnabledFunction::ManagementSetEnabledFunction() {
}

ManagementSetEnabledFunction::~ManagementSetEnabledFunction() {
}

ExtensionFunction::ResponseAction ManagementSetEnabledFunction::Run() {
  std::unique_ptr<management::SetEnabled::Params> params(
      management::SetEnabled::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());
  const ManagementAPIDelegate* delegate = ManagementAPI::GetFactoryInstance()
                                              ->Get(browser_context())
                                              ->GetDelegate();

  extension_id_ = params->id;

  const Extension* extension =
      registry->GetExtensionById(extension_id_, ExtensionRegistry::EVERYTHING);
  if (!extension || extension->ShouldNotBeVisible())
    return RespondNow(Error(keys::kNoExtensionError, extension_id_));

  bool enabled = params->enabled;
  const ManagementPolicy* policy =
      ExtensionSystem::Get(browser_context())->management_policy();
  if (!policy->UserMayModifySettings(extension, nullptr) ||
      (!enabled && policy->MustRemainEnabled(extension, nullptr)) ||
      (enabled && policy->MustRemainDisabled(extension, nullptr, nullptr))) {
    return RespondNow(Error(keys::kUserCantModifyError, extension_id_));
  }

  bool currently_enabled =
      registry->enabled_extensions().Contains(extension_id_) ||
      registry->terminated_extensions().Contains(extension_id_);

  if (!currently_enabled && enabled) {
    ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context());
    if (prefs->DidExtensionEscalatePermissions(extension_id_)) {
      if (!user_gesture())
        return RespondNow(Error(keys::kGestureNeededForEscalationError));

      AddRef();  // Matched in OnInstallPromptDone().
      install_prompt_ = delegate->SetEnabledFunctionDelegate(
          GetSenderWebContents(), browser_context(), extension,
          base::Bind(&ManagementSetEnabledFunction::OnInstallPromptDone, this));
      return RespondLater();
    }
    if (prefs->GetDisableReasons(extension_id_) &
            Extension::DISABLE_UNSUPPORTED_REQUIREMENT) {
      // Recheck the requirements.
      requirements_checker_ = delegate->CreateRequirementsChecker();
      requirements_checker_->Check(
          extension,
          base::Bind(&ManagementSetEnabledFunction::OnRequirementsChecked,
                     this));  // This bind creates a reference.
      return RespondLater();
    }
    delegate->EnableExtension(browser_context(), extension_id_);
  } else if (currently_enabled && !params->enabled) {
    delegate->DisableExtension(browser_context(), extension_id_,
                               Extension::DISABLE_USER_ACTION);
  }

  return RespondNow(NoArguments());
}

void ManagementSetEnabledFunction::OnInstallPromptDone(bool did_accept) {
  if (did_accept) {
    ManagementAPI::GetFactoryInstance()
        ->Get(browser_context())
        ->GetDelegate()
        ->EnableExtension(browser_context(), extension_id_);
    Respond(OneArgument(base::MakeUnique<base::FundamentalValue>(true)));
  } else {
    Respond(Error(keys::kUserDidNotReEnableError));
  }

  Release();  // Balanced in Run().
}

void ManagementSetEnabledFunction::OnRequirementsChecked(
    const std::vector<std::string>& requirements_errors) {
  if (requirements_errors.empty()) {
    ManagementAPI::GetFactoryInstance()->Get(browser_context())->GetDelegate()->
        EnableExtension(browser_context(), extension_id_);
    Respond(NoArguments());
  } else {
    // TODO(devlin): Should we really be noisy here all the time?
    Respond(Error(keys::kMissingRequirementsError,
                  base::JoinString(requirements_errors, " ")));
  }
}

ManagementUninstallFunctionBase::ManagementUninstallFunctionBase() {
}

ManagementUninstallFunctionBase::~ManagementUninstallFunctionBase() {
}

ExtensionFunction::ResponseAction ManagementUninstallFunctionBase::Uninstall(
    const std::string& target_extension_id,
    bool show_confirm_dialog) {
  const ManagementAPIDelegate* delegate = ManagementAPI::GetFactoryInstance()
                                              ->Get(browser_context())
                                              ->GetDelegate();
  target_extension_id_ = target_extension_id;
  const Extension* target_extension =
      extensions::ExtensionRegistry::Get(browser_context())
          ->GetExtensionById(target_extension_id_,
                             ExtensionRegistry::EVERYTHING);
  if (!target_extension || target_extension->ShouldNotBeVisible()) {
    return RespondNow(Error(keys::kNoExtensionError, target_extension_id_));
  }

  ManagementPolicy* policy =
      ExtensionSystem::Get(browser_context())->management_policy();
  if (!policy->UserMayModifySettings(target_extension, nullptr) ||
      policy->MustRemainInstalled(target_extension, nullptr)) {
    return RespondNow(Error(keys::kUserCantModifyError, target_extension_id_));
  }

  // Note: null extension() means it's WebUI.
  bool self_uninstall = extension() && extension_id() == target_extension_id_;
  // We need to show a dialog for any extension uninstalling another extension.
  show_confirm_dialog |= !self_uninstall;

  if (show_confirm_dialog && !user_gesture())
    return RespondNow(Error(keys::kGestureNeededForUninstallError));

  if (show_confirm_dialog) {
    // We show the programmatic uninstall ui for extensions uninstalling
    // other extensions.
    bool show_programmatic_uninstall_ui = !self_uninstall && extension();
    AddRef();  // Balanced in OnExtensionUninstallDialogClosed.
    // TODO(devlin): A method called "UninstallFunctionDelegate" does not in
    // any way imply that this actually creates a dialog and runs it.
    uninstall_dialog_ = delegate->UninstallFunctionDelegate(
        this, target_extension, show_programmatic_uninstall_ui);
  } else {  // No confirm dialog.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&ManagementUninstallFunctionBase::UninstallExtension, this));
  }

  return RespondLater();
}

void ManagementUninstallFunctionBase::Finish(bool did_start_uninstall,
                                             const std::string& error) {
  Respond(did_start_uninstall ? NoArguments() : Error(error));
}

void ManagementUninstallFunctionBase::OnExtensionUninstallDialogClosed(
    bool did_start_uninstall,
    const base::string16& error) {
  Finish(did_start_uninstall,
         ErrorUtils::FormatErrorMessage(keys::kUninstallCanceledError,
                                        target_extension_id_));
  Release();  // Balanced in Uninstall().
}

void ManagementUninstallFunctionBase::UninstallExtension() {
  // The extension can be uninstalled in another window while the UI was
  // showing. Do nothing in that case.
  const Extension* target_extension =
      extensions::ExtensionRegistry::Get(browser_context())
          ->GetExtensionById(target_extension_id_,
                             ExtensionRegistry::EVERYTHING);
  std::string error;
  bool success = false;
  if (target_extension) {
    const ManagementAPIDelegate* delegate = ManagementAPI::GetFactoryInstance()
                                                ->Get(browser_context())
                                                ->GetDelegate();
    base::string16 utf16_error;
    success = delegate->UninstallExtension(
        browser_context(), target_extension_id_,
        extensions::UNINSTALL_REASON_MANAGEMENT_API,
        base::Bind(&base::DoNothing), &utf16_error);
    error = base::UTF16ToUTF8(utf16_error);
  } else {
    error = ErrorUtils::FormatErrorMessage(keys::kNoExtensionError,
                                           target_extension_id_);
  }
  Finish(success, error);
}

ManagementUninstallFunction::ManagementUninstallFunction() {
}

ManagementUninstallFunction::~ManagementUninstallFunction() {
}

ExtensionFunction::ResponseAction ManagementUninstallFunction::Run() {
  std::unique_ptr<management::Uninstall::Params> params(
      management::Uninstall::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  bool show_confirm_dialog = params->options.get() &&
                             params->options->show_confirm_dialog.get() &&
                             *params->options->show_confirm_dialog;
  return Uninstall(params->id, show_confirm_dialog);
}

ManagementUninstallSelfFunction::ManagementUninstallSelfFunction() {
}

ManagementUninstallSelfFunction::~ManagementUninstallSelfFunction() {
}

ExtensionFunction::ResponseAction ManagementUninstallSelfFunction::Run() {
  std::unique_ptr<management::UninstallSelf::Params> params(
      management::UninstallSelf::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  EXTENSION_FUNCTION_VALIDATE(extension_.get());

  bool show_confirm_dialog = params->options.get() &&
                             params->options->show_confirm_dialog.get() &&
                             *params->options->show_confirm_dialog;
  return Uninstall(extension_->id(), show_confirm_dialog);
}

ManagementCreateAppShortcutFunction::ManagementCreateAppShortcutFunction() {
}

ManagementCreateAppShortcutFunction::~ManagementCreateAppShortcutFunction() {
}

// static
void ManagementCreateAppShortcutFunction::SetAutoConfirmForTest(
    bool should_proceed) {
  auto_confirm_for_test = should_proceed ? PROCEED : ABORT;
}

void ManagementCreateAppShortcutFunction::OnCloseShortcutPrompt(bool created) {
  if (!created)
    error_ = keys::kCreateShortcutCanceledError;
  SendResponse(created);
  Release();
}

bool ManagementCreateAppShortcutFunction::RunAsync() {
  if (!user_gesture()) {
    error_ = keys::kGestureNeededForCreateAppShortcutError;
    return false;
  }

  std::unique_ptr<management::CreateAppShortcut::Params> params(
      management::CreateAppShortcut::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  const Extension* extension =
      ExtensionRegistry::Get(browser_context())
          ->GetExtensionById(params->id, ExtensionRegistry::EVERYTHING);
  if (!extension) {
    error_ =
        ErrorUtils::FormatErrorMessage(keys::kNoExtensionError, params->id);
    return false;
  }

  if (!extension->is_app()) {
    error_ = ErrorUtils::FormatErrorMessage(keys::kNotAnAppError, params->id);
    return false;
  }

#if defined(OS_MACOSX)
  if (!extension->is_platform_app()) {
    error_ = keys::kCreateOnlyPackagedAppShortcutMac;
    return false;
  }
#endif

  if (auto_confirm_for_test != DO_NOT_SKIP) {
    // Matched with a Release() in OnCloseShortcutPrompt().
    AddRef();

    OnCloseShortcutPrompt(auto_confirm_for_test == PROCEED);

    return true;
  }

  if (ManagementAPI::GetFactoryInstance()
          ->Get(browser_context())
          ->GetDelegate()
          ->CreateAppShortcutFunctionDelegate(this, extension)) {
    // Matched with a Release() in OnCloseShortcutPrompt().
    AddRef();
  }

  // Response is sent async in OnCloseShortcutPrompt().
  return true;
}

bool ManagementSetLaunchTypeFunction::RunSync() {
  if (!user_gesture()) {
    error_ = keys::kGestureNeededForSetLaunchTypeError;
    return false;
  }

  std::unique_ptr<management::SetLaunchType::Params> params(
      management::SetLaunchType::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  const Extension* extension =
      ExtensionRegistry::Get(browser_context())
          ->GetExtensionById(params->id, ExtensionRegistry::EVERYTHING);
  const ManagementAPIDelegate* delegate = ManagementAPI::GetFactoryInstance()
                                              ->Get(browser_context())
                                              ->GetDelegate();
  if (!extension) {
    error_ =
        ErrorUtils::FormatErrorMessage(keys::kNoExtensionError, params->id);
    return false;
  }

  if (!extension->is_app()) {
    error_ = ErrorUtils::FormatErrorMessage(keys::kNotAnAppError, params->id);
    return false;
  }

  std::vector<management::LaunchType> available_launch_types =
      GetAvailableLaunchTypes(*extension, delegate);

  management::LaunchType app_launch_type = params->launch_type;
  if (std::find(available_launch_types.begin(), available_launch_types.end(),
                app_launch_type) == available_launch_types.end()) {
    error_ = keys::kLaunchTypeNotAvailableError;
    return false;
  }

  LaunchType launch_type = LAUNCH_TYPE_DEFAULT;
  switch (app_launch_type) {
    case management::LAUNCH_TYPE_OPEN_AS_PINNED_TAB:
      launch_type = LAUNCH_TYPE_PINNED;
      break;
    case management::LAUNCH_TYPE_OPEN_AS_REGULAR_TAB:
      launch_type = LAUNCH_TYPE_REGULAR;
      break;
    case management::LAUNCH_TYPE_OPEN_FULL_SCREEN:
      launch_type = LAUNCH_TYPE_FULLSCREEN;
      break;
    case management::LAUNCH_TYPE_OPEN_AS_WINDOW:
      launch_type = LAUNCH_TYPE_WINDOW;
      break;
    case management::LAUNCH_TYPE_NONE:
      NOTREACHED();
  }

  delegate->SetLaunchType(browser_context(), params->id, launch_type);

  return true;
}

ManagementGenerateAppForLinkFunction::ManagementGenerateAppForLinkFunction() {
}

ManagementGenerateAppForLinkFunction::~ManagementGenerateAppForLinkFunction() {
}

void ManagementGenerateAppForLinkFunction::FinishCreateBookmarkApp(
    const Extension* extension,
    const WebApplicationInfo& web_app_info) {
  if (extension) {
    results_ = management::GenerateAppForLink::Results::Create(
        CreateExtensionInfo(*extension, browser_context()));

    SendResponse(true);
    Release();
  } else {
    error_ = keys::kGenerateAppForLinkInstallError;
    SendResponse(false);
    Release();
  }
}

bool ManagementGenerateAppForLinkFunction::RunAsync() {
  if (!user_gesture()) {
    error_ = keys::kGestureNeededForGenerateAppForLinkError;
    return false;
  }

  std::unique_ptr<management::GenerateAppForLink::Params> params(
      management::GenerateAppForLink::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  GURL launch_url(params->url);
  if (!launch_url.is_valid() || !launch_url.SchemeIsHTTPOrHTTPS()) {
    error_ =
        ErrorUtils::FormatErrorMessage(keys::kInvalidURLError, params->url);
    return false;
  }

  if (params->title.empty()) {
    error_ = keys::kEmptyTitleError;
    return false;
  }

  app_for_link_delegate_ =
      ManagementAPI::GetFactoryInstance()
          ->Get(browser_context())
          ->GetDelegate()
          ->GenerateAppForLinkFunctionDelegate(this, browser_context(),
                                               params->title, launch_url);

  // Matched with a Release() in FinishCreateBookmarkApp().
  AddRef();

  // Response is sent async in FinishCreateBookmarkApp().
  return true;
}

ManagementEventRouter::ManagementEventRouter(content::BrowserContext* context)
    : browser_context_(context), extension_registry_observer_(this) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context_));
}

ManagementEventRouter::~ManagementEventRouter() {
}

void ManagementEventRouter::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  BroadcastEvent(extension, events::MANAGEMENT_ON_ENABLED,
                 management::OnEnabled::kEventName);
}

void ManagementEventRouter::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  BroadcastEvent(extension, events::MANAGEMENT_ON_DISABLED,
                 management::OnDisabled::kEventName);
}

void ManagementEventRouter::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update) {
  BroadcastEvent(extension, events::MANAGEMENT_ON_INSTALLED,
                 management::OnInstalled::kEventName);
}

void ManagementEventRouter::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  BroadcastEvent(extension, events::MANAGEMENT_ON_UNINSTALLED,
                 management::OnUninstalled::kEventName);
}

void ManagementEventRouter::BroadcastEvent(
    const Extension* extension,
    events::HistogramValue histogram_value,
    const char* event_name) {
  if (extension->ShouldNotBeVisible())
    return;  // Don't dispatch events for built-in extenions.
  std::unique_ptr<base::ListValue> args(new base::ListValue());
  if (event_name == management::OnUninstalled::kEventName) {
    args->AppendString(extension->id());
  } else {
    args->Append(CreateExtensionInfo(*extension, browser_context_).ToValue());
  }

  EventRouter::Get(browser_context_)
      ->BroadcastEvent(std::unique_ptr<Event>(
          new Event(histogram_value, event_name, std::move(args))));
}

ManagementAPI::ManagementAPI(content::BrowserContext* context)
    : browser_context_(context),
      delegate_(ExtensionsAPIClient::Get()->CreateManagementAPIDelegate()) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(this, management::OnInstalled::kEventName);
  event_router->RegisterObserver(this, management::OnUninstalled::kEventName);
  event_router->RegisterObserver(this, management::OnEnabled::kEventName);
  event_router->RegisterObserver(this, management::OnDisabled::kEventName);
}

ManagementAPI::~ManagementAPI() {
}

void ManagementAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<ManagementAPI>>
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<ManagementAPI>*
ManagementAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

void ManagementAPI::OnListenerAdded(const EventListenerInfo& details) {
  management_event_router_.reset(new ManagementEventRouter(browser_context_));
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

}  // namespace extensions
