// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_DEFAULT_COMPONENT_INSTALLER_H_
#define COMPONENTS_COMPONENT_UPDATER_DEFAULT_COMPONENT_INSTALLER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "base/values.h"
#include "base/version.h"
#include "components/update_client/update_client.h"

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}  // namespace base

namespace component_updater {

class ComponentUpdateService;

// Components should use a DefaultComponentInstaller by defining a class that
// implements the members of ComponentInstallerTraits, and then registering a
// DefaultComponentInstaller that has been constructed with an instance of that
// class.
class ComponentInstallerTraits {
 public:
  virtual ~ComponentInstallerTraits();

  // Verifies that a working installation resides within the directory specified
  // by |install_dir|. |install_dir| is of the form <base directory>/<version>.
  // |manifest| should have been read from the manifest file in |install_dir|.
  // Called only from a thread belonging to a blocking thread pool.
  // The implementation of this function must be efficient since the function
  // can be called when Chrome starts.
  virtual bool VerifyInstallation(const base::DictionaryValue& manifest,
                                  const base::FilePath& install_dir) const = 0;

  // Returns true if the component supports a group policy to enable updates.
  // Called once during component registration from the UI thread.
  virtual bool SupportsGroupPolicyEnabledComponentUpdates() const = 0;

  // Returns true if the network communication related to this component
  // must be encrypted.
  virtual bool RequiresNetworkEncryption() const = 0;

  // OnCustomInstall is called during the installation process. Components that
  // require custom installation operations should implement them here.
  // Returns false if a custom operation failed, and true otherwise.
  // Called only from a thread belonging to a blocking thread pool.
  virtual update_client::CrxInstaller::Result OnCustomInstall(
      const base::DictionaryValue& manifest,
      const base::FilePath& install_dir) = 0;

  // ComponentReady is called in two cases:
  //   1) After an installation is successfully completed.
  //   2) During component registration if the component is already installed.
  // In both cases the install is verified before this is called. This method
  // is guaranteed to be called before any observers of the component are
  // notified of a successful install, and is meant to support follow-on work
  // such as updating paths elsewhere in Chrome. Called on the UI thread.
  // |version| is the version of the component.
  // |install_dir| is the path to the install directory for this version.
  // |manifest| is the manifest for this version of the component.
  virtual void ComponentReady(
      const base::Version& version,
      const base::FilePath& install_dir,
      std::unique_ptr<base::DictionaryValue> manifest) = 0;

  // Returns a relative path that will be appended to the component updater
  // root directories to find the data for this particular component.
  virtual base::FilePath GetRelativeInstallDir() const = 0;

  // Returns the component's SHA2 hash as raw bytes.
  virtual void GetHash(std::vector<uint8_t>* hash) const = 0;

  // Returns the human-readable name of the component.
  virtual std::string GetName() const = 0;

  // If this component is a plugin, returns the media types it can handle.
  virtual std::vector<std::string> GetMimeTypes() const = 0;

  // Returns a container of name-value pairs representing arbitrary,
  // installer-defined metadata.
  // The installer metadata may be used in the update checks for this component.
  // A compatible server may use these attributes to negotiate special update
  // rules when issuing an update response.
  // Valid values for the name part of an attribute match
  // ^[-_a-zA-Z0-9]{1,256}$ and valid values the value part of an attribute
  // match ^[-.,;+_=a-zA-Z0-9]{0,256}$ .
  virtual update_client::InstallerAttributes GetInstallerAttributes() const = 0;
};

// A DefaultComponentInstaller is intended to be final, and not derived from.
// Customization must be provided by passing a ComponentInstallerTraits object
// to the constructor.
class DefaultComponentInstaller : public update_client::CrxInstaller {
 public:
  DefaultComponentInstaller(
      std::unique_ptr<ComponentInstallerTraits> installer_traits);

  // Registers the component for update checks and installs.
  // The passed |callback| will be called once the initial check for installed
  // versions is done and the component has been registered.
  void Register(ComponentUpdateService* cus, const base::Closure& callback);

  // Overridden from ComponentInstaller:
  void OnUpdateError(int error) override;
  update_client::CrxInstaller::Result Install(
      const base::DictionaryValue& manifest,
      const base::FilePath& unpack_path) override;
  bool GetInstalledFile(const std::string& file,
                        base::FilePath* installed_file) override;
  // Only user-level component installations can be uninstalled.
  bool Uninstall() override;

 private:
  ~DefaultComponentInstaller() override;

  // If there is a installation of the component set up alongside Chrome's
  // files (as opposed to in the user data directory), sets current_* to the
  // values associated with that installation and returns true; otherwise,
  // returns false.
  bool FindPreinstallation(const base::FilePath& root);
  update_client::CrxInstaller::Result InstallHelper(
      const base::DictionaryValue& manifest,
      const base::FilePath& unpack_path,
      const base::FilePath& install_path);
  void StartRegistration(ComponentUpdateService* cus);
  void FinishRegistration(ComponentUpdateService* cus,
                          const base::Closure& callback);
  void ComponentReady(std::unique_ptr<base::DictionaryValue> manifest);
  void UninstallOnTaskRunner();

  base::FilePath current_install_dir_;
  base::Version current_version_;
  std::string current_fingerprint_;
  std::unique_ptr<base::DictionaryValue> current_manifest_;
  std::unique_ptr<ComponentInstallerTraits> installer_traits_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Used to post responses back to the main thread. Initialized on the main
  // loop but accessed from the task runner.
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(DefaultComponentInstaller);
};

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_DEFAULT_COMPONENT_INSTALLER_H_
