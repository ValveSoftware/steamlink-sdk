// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_extension_system.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "extensions/browser/api/app_runtime/app_runtime_api.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/null_app_sorting.h"
#include "extensions/browser/quota_service.h"
#include "extensions/browser/runtime_data.h"
#include "extensions/browser/service_worker_manager.h"
#include "extensions/browser/value_store/value_store_factory_impl.h"
#include "extensions/common/constants.h"
#include "extensions/common/file_util.h"

using content::BrowserContext;
using content::BrowserThread;

namespace extensions {

ShellExtensionSystem::ShellExtensionSystem(BrowserContext* browser_context)
    : browser_context_(browser_context),
      store_factory_(new ValueStoreFactoryImpl(browser_context->GetPath())),
      weak_factory_(this) {}

ShellExtensionSystem::~ShellExtensionSystem() {
}

const Extension* ShellExtensionSystem::LoadApp(const base::FilePath& app_dir) {
  // app_shell only supports unpacked extensions.
  // NOTE: If you add packed extension support consider removing the flag
  // FOLLOW_SYMLINKS_ANYWHERE below. Packed extensions should not have symlinks.
  CHECK(base::DirectoryExists(app_dir)) << app_dir.AsUTF8Unsafe();
  int load_flags = Extension::FOLLOW_SYMLINKS_ANYWHERE;
  std::string load_error;
  scoped_refptr<Extension> extension = file_util::LoadExtension(
      app_dir, Manifest::COMMAND_LINE, load_flags, &load_error);
  if (!extension.get()) {
    LOG(ERROR) << "Loading extension at " << app_dir.value()
               << " failed with: " << load_error;
    return nullptr;
  }

  // TODO(jamescook): We may want to do some of these things here:
  // * Create a PermissionsUpdater.
  // * Call PermissionsUpdater::GrantActivePermissions().
  // * Call ExtensionService::SatisfyImports().
  // * Call ExtensionPrefs::OnExtensionInstalled().
  // * Call ExtensionRegistryObserver::OnExtensionWillbeInstalled().

  ExtensionRegistry::Get(browser_context_)->AddEnabled(extension.get());

  RegisterExtensionWithRequestContexts(
      extension.get(),
      base::Bind(
          &ShellExtensionSystem::OnExtensionRegisteredWithRequestContexts,
          weak_factory_.GetWeakPtr(), extension));

  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_LOADED_DEPRECATED,
      content::Source<BrowserContext>(browser_context_),
      content::Details<const Extension>(extension.get()));

  return extension.get();
}

void ShellExtensionSystem::Init() {
  // Inform the rest of the extensions system to start.
  ready_.Signal();
  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSIONS_READY_DEPRECATED,
      content::Source<BrowserContext>(browser_context_),
      content::NotificationService::NoDetails());
}

void ShellExtensionSystem::LaunchApp(const ExtensionId& extension_id) {
  // Send the onLaunched event.
  DCHECK(ExtensionRegistry::Get(browser_context_)
             ->enabled_extensions()
             .Contains(extension_id));
  const Extension* extension = ExtensionRegistry::Get(browser_context_)
                                   ->enabled_extensions()
                                   .GetByID(extension_id);
  AppRuntimeEventRouter::DispatchOnLaunchedEvent(
      browser_context_, extension, extensions::SOURCE_UNTRACKED);
}

void ShellExtensionSystem::Shutdown() {
}

void ShellExtensionSystem::InitForRegularProfile(bool extensions_enabled) {
  service_worker_manager_.reset(new ServiceWorkerManager(browser_context_));
  runtime_data_.reset(
      new RuntimeData(ExtensionRegistry::Get(browser_context_)));
  quota_service_.reset(new QuotaService);
  app_sorting_.reset(new NullAppSorting);
}

ExtensionService* ShellExtensionSystem::extension_service() {
  return nullptr;
}

RuntimeData* ShellExtensionSystem::runtime_data() {
  return runtime_data_.get();
}

ManagementPolicy* ShellExtensionSystem::management_policy() {
  return nullptr;
}

ServiceWorkerManager* ShellExtensionSystem::service_worker_manager() {
  return service_worker_manager_.get();
}

SharedUserScriptMaster* ShellExtensionSystem::shared_user_script_master() {
  return nullptr;
}

StateStore* ShellExtensionSystem::state_store() {
  return nullptr;
}

StateStore* ShellExtensionSystem::rules_store() {
  return nullptr;
}

scoped_refptr<ValueStoreFactory> ShellExtensionSystem::store_factory() {
  return store_factory_;
}

InfoMap* ShellExtensionSystem::info_map() {
  if (!info_map_.get())
    info_map_ = new InfoMap;
  return info_map_.get();
}

QuotaService* ShellExtensionSystem::quota_service() {
  return quota_service_.get();
}

AppSorting* ShellExtensionSystem::app_sorting() {
  return app_sorting_.get();
}

void ShellExtensionSystem::RegisterExtensionWithRequestContexts(
    const Extension* extension,
    const base::Closure& callback) {
  BrowserThread::PostTaskAndReply(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&InfoMap::AddExtension, info_map(),
                 base::RetainedRef(extension), base::Time::Now(), false, false),
      callback);
}

void ShellExtensionSystem::UnregisterExtensionWithRequestContexts(
    const std::string& extension_id,
    const UnloadedExtensionInfo::Reason reason) {
}

const OneShotEvent& ShellExtensionSystem::ready() const {
  return ready_;
}

ContentVerifier* ShellExtensionSystem::content_verifier() {
  return nullptr;
}

std::unique_ptr<ExtensionSet> ShellExtensionSystem::GetDependentExtensions(
    const Extension* extension) {
  return base::WrapUnique(new ExtensionSet());
}

void ShellExtensionSystem::InstallUpdate(const std::string& extension_id,
                                         const base::FilePath& temp_dir) {
  NOTREACHED();
  base::DeleteFile(temp_dir, true /* recursive */);
}

void ShellExtensionSystem::OnExtensionRegisteredWithRequestContexts(
    scoped_refptr<Extension> extension) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context_);
  registry->AddReady(extension);
  registry->TriggerOnReady(extension.get());
}

}  // namespace extensions
