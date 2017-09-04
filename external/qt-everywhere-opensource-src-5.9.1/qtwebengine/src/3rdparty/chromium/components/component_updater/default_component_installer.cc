// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/default_component_installer.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/component_updater_paths.h"
// TODO(ddorwin): Find a better place for ReadManifest.
#include "components/component_updater/component_updater_service.h"
#include "components/update_client/component_unpacker.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/utils.h"

namespace component_updater {

namespace {

// Version "0" corresponds to no installed version. By the server's conventions,
// we represent it as a dotted quad.
const char kNullVersion[] = "0.0.0.0";

using Result = update_client::CrxInstaller::Result;
using InstallError = update_client::InstallError;

}  // namespace

ComponentInstallerTraits::~ComponentInstallerTraits() {
}

DefaultComponentInstaller::DefaultComponentInstaller(
    std::unique_ptr<ComponentInstallerTraits> installer_traits)
    : current_version_(kNullVersion),
      main_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  installer_traits_ = std::move(installer_traits);
}

DefaultComponentInstaller::~DefaultComponentInstaller() {
}

void DefaultComponentInstaller::Register(
    ComponentUpdateService* cus,
    const base::Closure& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  task_runner_ = cus->GetSequencedTaskRunner();

  if (!installer_traits_) {
    LOG(ERROR) << "A DefaultComponentInstaller has been created but "
               << "has no installer traits.";
    return;
  }
  task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&DefaultComponentInstaller::StartRegistration,
                 this, cus),
      base::Bind(&DefaultComponentInstaller::FinishRegistration,
                 this, cus, callback));
}

void DefaultComponentInstaller::OnUpdateError(int error) {
  LOG(ERROR) << "Component update error: " << error;
}

Result DefaultComponentInstaller::InstallHelper(
    const base::DictionaryValue& manifest,
    const base::FilePath& unpack_path,
    const base::FilePath& install_path) {
  VLOG(1) << "InstallHelper: unpack_path=" << unpack_path.AsUTF8Unsafe()
          << " install_path=" << install_path.AsUTF8Unsafe();

  if (!base::Move(unpack_path, install_path)) {
    PLOG(ERROR) << "Move failed.";
    return Result(InstallError::MOVE_FILES_ERROR);
  }

#if defined(OS_CHROMEOS)
  if (!base::SetPosixFilePermissions(install_path, 0755)) {
    PLOG(ERROR) << "SetPosixFilePermissions failed: " << install_path.value();
    return Result(InstallError::SET_PERMISSIONS_FAILED);
  }
#endif  // defined(OS_CHROMEOS)

  const auto result =
      installer_traits_->OnCustomInstall(manifest, install_path);
  if (result.error) {
    PLOG(ERROR) << "CustomInstall failed.";
    return result;
  }
  if (!installer_traits_->VerifyInstallation(manifest, install_path)) {
    return Result(InstallError::INSTALL_VERIFICATION_FAILED);
  }

  return Result(InstallError::NONE);
}

Result DefaultComponentInstaller::Install(const base::DictionaryValue& manifest,
                                          const base::FilePath& unpack_path) {
  std::string manifest_version;
  manifest.GetStringASCII("version", &manifest_version);
  base::Version version(manifest_version);

  VLOG(1) << "Install: version=" << version.GetString()
          << " current version=" << current_version_.GetString();

  if (!version.IsValid())
    return Result(InstallError::INVALID_VERSION);
  if (current_version_.CompareTo(version) > 0)
    return Result(InstallError::VERSION_NOT_UPGRADED);
  base::FilePath install_path;
  if (!PathService::Get(DIR_COMPONENT_USER, &install_path))
    return Result(InstallError::NO_DIR_COMPONENT_USER);
  install_path = install_path.Append(installer_traits_->GetRelativeInstallDir())
                     .AppendASCII(version.GetString());
  if (base::PathExists(install_path)) {
    if (!base::DeleteFile(install_path, true))
      return Result(InstallError::CLEAN_INSTALL_DIR_FAILED);
  }
  const auto result = InstallHelper(manifest, unpack_path, install_path);
  if (result.error) {
    base::DeleteFile(install_path, true);
    return result;
  }
  current_version_ = version;
  current_install_dir_ = install_path;
  // TODO(ddorwin): Change parameter to std::unique_ptr<base::DictionaryValue>
  // so we can avoid this DeepCopy.
  current_manifest_.reset(manifest.DeepCopy());
  std::unique_ptr<base::DictionaryValue> manifest_copy(
      current_manifest_->DeepCopy());
  main_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DefaultComponentInstaller::ComponentReady,
                 this, base::Passed(&manifest_copy)));
  return result;
}

bool DefaultComponentInstaller::GetInstalledFile(
    const std::string& file,
    base::FilePath* installed_file) {
  if (current_version_ == base::Version(kNullVersion))
    return false;  // No component has been installed yet.
  *installed_file = current_install_dir_.AppendASCII(file);
  return true;
}

bool DefaultComponentInstaller::Uninstall() {
  DCHECK(thread_checker_.CalledOnValidThread());
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&DefaultComponentInstaller::UninstallOnTaskRunner, this));
  return true;
}

bool DefaultComponentInstaller::FindPreinstallation(
    const base::FilePath& root) {
  base::FilePath path = root.Append(installer_traits_->GetRelativeInstallDir());
  if (!base::PathExists(path)) {
    DVLOG(1) << "Relative install dir does not exist: " << path.MaybeAsASCII();
    return false;
  }

  std::unique_ptr<base::DictionaryValue> manifest =
      update_client::ReadManifest(path);
  if (!manifest) {
    DVLOG(1) << "Manifest does not exist: " << path.MaybeAsASCII();
    return false;
  }

  if (!installer_traits_->VerifyInstallation(*manifest, path)) {
    DVLOG(1) << "Installation verification failed: " << path.MaybeAsASCII();
    return false;
  }

  std::string version_lexical;
  if (!manifest->GetStringASCII("version", &version_lexical)) {
    DVLOG(1) << "Failed to get component version from the manifest.";
    return false;
  }

  const base::Version version(version_lexical);
  if (!version.IsValid()) {
    DVLOG(1) << "Version in the manifest is invalid:" << version_lexical;
    return false;
  }

  VLOG(1) << "Preinstalled component found for " << installer_traits_->GetName()
          << " at " << path.MaybeAsASCII() << " with version " << version
          << ".";

  current_install_dir_ = path;
  current_manifest_ = std::move(manifest);
  current_version_ = version;
  return true;
}

void DefaultComponentInstaller::StartRegistration(ComponentUpdateService* cus) {
  VLOG(1) << __func__ << " for " << installer_traits_->GetName();
  DCHECK(task_runner_.get());
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  base::Version latest_version(kNullVersion);

  // First check for an installation set up alongside Chrome itself.
  base::FilePath root;
  if (PathService::Get(DIR_COMPONENT_PREINSTALLED, &root) &&
      FindPreinstallation(root)) {
    latest_version = current_version_;
  }

  // If there is a distinct alternate root, check there as well, and override
  // anything found in the basic root.
  base::FilePath root_alternate;
  if (PathService::Get(DIR_COMPONENT_PREINSTALLED_ALT, &root_alternate) &&
      root != root_alternate && FindPreinstallation(root_alternate)) {
    latest_version = current_version_;
  }

  // Then check for a higher-versioned user-wide installation.
  base::FilePath latest_path;
  std::unique_ptr<base::DictionaryValue> latest_manifest;
  base::FilePath base_dir;
  if (!PathService::Get(DIR_COMPONENT_USER, &base_dir))
    return;
  base_dir = base_dir.Append(installer_traits_->GetRelativeInstallDir());
  if (!base::PathExists(base_dir) && !base::CreateDirectory(base_dir)) {
    PLOG(ERROR) << "Could not create the base directory for "
                << installer_traits_->GetName() << " ("
                << base_dir.MaybeAsASCII() << ").";
    return;
  }

#if defined(OS_CHROMEOS)
  if (!base::SetPosixFilePermissions(base_dir, 0755)) {
    PLOG(ERROR) << "SetPosixFilePermissions failed: " << base_dir.value();
    return;
  }
#endif  // defined(OS_CHROMEOS)

  std::vector<base::FilePath> older_paths;
  base::FileEnumerator file_enumerator(
      base_dir, false, base::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = file_enumerator.Next();
       !path.value().empty();
       path = file_enumerator.Next()) {
    base::Version version(path.BaseName().MaybeAsASCII());

    // Ignore folders that don't have valid version names. These folders are not
    // managed by component installer so do not try to remove them.
    if (!version.IsValid())
      continue;

    // |version| not newer than the latest found version (kNullVersion if no
    // version has been found yet) is marked for removal.
    if (version.CompareTo(latest_version) <= 0) {
      older_paths.push_back(path);
      continue;
    }

    std::unique_ptr<base::DictionaryValue> manifest =
        update_client::ReadManifest(path);
    if (!manifest || !installer_traits_->VerifyInstallation(*manifest, path)) {
      PLOG(ERROR) << "Failed to read manifest or verify installation for "
                  << installer_traits_->GetName() << " (" << path.MaybeAsASCII()
                  << ").";
      older_paths.push_back(path);
      continue;
    }

    // New valid |version| folder found!

    if (!latest_path.empty())
      older_paths.push_back(latest_path);

    latest_path = path;
    latest_version = version;
    latest_manifest = std::move(manifest);
  }

  if (latest_manifest) {
    current_version_ = latest_version;
    current_manifest_ = std::move(latest_manifest);
    current_install_dir_ = latest_path;
    // TODO(ddorwin): Remove these members and pass them directly to
    // FinishRegistration().
    base::ReadFileToString(latest_path.AppendASCII("manifest.fingerprint"),
                           &current_fingerprint_);
  }

  // Remove older versions of the component. None should be in use during
  // browser startup.
  for (const auto& older_path : older_paths)
    base::DeleteFile(older_path, true);
}

void DefaultComponentInstaller::UninstallOnTaskRunner() {
  DCHECK(task_runner_.get());
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  // Only try to delete any files that are in our user-level install path.
  base::FilePath userInstallPath;
  if (!PathService::Get(DIR_COMPONENT_USER, &userInstallPath))
    return;
  if (!userInstallPath.IsParent(current_install_dir_))
    return;

  const base::FilePath base_dir = current_install_dir_.DirName();
  base::FileEnumerator file_enumerator(base_dir, false,
                                       base::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = file_enumerator.Next(); !path.value().empty();
       path = file_enumerator.Next()) {
    base::Version version(path.BaseName().MaybeAsASCII());

    // Ignore folders that don't have valid version names. These folders are not
    // managed by the component installer, so do not try to remove them.
    if (!version.IsValid())
      continue;

    if (!base::DeleteFile(path, true))
      DLOG(ERROR) << "Couldn't delete " << path.value();
  }

  // Delete the base directory if it's empty now.
  if (base::IsDirectoryEmpty(base_dir)) {
    if (base::DeleteFile(base_dir, false))
      DLOG(ERROR) << "Couldn't delete " << base_dir.value();
  }
}

void DefaultComponentInstaller::FinishRegistration(
    ComponentUpdateService* cus,
    const base::Closure& callback) {
  VLOG(1) << __func__ << " for " << installer_traits_->GetName();
  DCHECK(thread_checker_.CalledOnValidThread());

  update_client::CrxComponent crx;
  installer_traits_->GetHash(&crx.pk_hash);
  crx.installer = this;
  crx.version = current_version_;
  crx.fingerprint = current_fingerprint_;
  crx.name = installer_traits_->GetName();
  crx.installer_attributes = installer_traits_->GetInstallerAttributes();
  crx.requires_network_encryption =
      installer_traits_->RequiresNetworkEncryption();
  crx.handled_mime_types = installer_traits_->GetMimeTypes();
  crx.supports_group_policy_enable_component_updates =
      installer_traits_->SupportsGroupPolicyEnabledComponentUpdates();

  if (!cus->RegisterComponent(crx)) {
    LOG(ERROR) << "Component registration failed for "
               << installer_traits_->GetName();
    return;
  }

  if (!callback.is_null())
    callback.Run();

  if (!current_manifest_) {
    DVLOG(1) << "No component found for " << installer_traits_->GetName();
    return;
  }

  std::unique_ptr<base::DictionaryValue> manifest_copy(
      current_manifest_->DeepCopy());
  ComponentReady(std::move(manifest_copy));
}

void DefaultComponentInstaller::ComponentReady(
    std::unique_ptr<base::DictionaryValue> manifest) {
  VLOG(1) << "Component ready, version " << current_version_.GetString()
          << " in " << current_install_dir_.value();
  installer_traits_->ComponentReady(current_version_, current_install_dir_,
                                    std::move(manifest));
}

}  // namespace component_updater
