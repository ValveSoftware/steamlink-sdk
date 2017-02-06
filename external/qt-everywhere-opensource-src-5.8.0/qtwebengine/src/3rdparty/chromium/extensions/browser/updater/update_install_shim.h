// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UPDATER_UPDATE_INSTALL_SHIM_H_
#define EXTENSIONS_BROWSER_UPDATER_UPDATE_INSTALL_SHIM_H_

#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "components/update_client/update_client.h"

namespace base {
class DictionaryValue;
}

namespace extensions {

// A callback to implement the install of a new version of the extension.
// Takes ownership of the directory at |temp_dir|.
using UpdateInstallShimCallback =
    base::Callback<void(const std::string& extension_id,
                        const base::FilePath& temp_dir)>;

// This class is used as a shim between the components::update_client and
// extensions code, to help the generic update_client code prepare and then
// install an updated version of an extension. Because the update_client code
// doesn't have the notion of extension ids, we use instances of this class to
// map an install request back to the original update check for a given
// extension.
class UpdateInstallShim : public update_client::CrxInstaller {
 public:
  // This method takes the id and root directory for an extension we're doing
  // an update check for, as well as a callback to call if we get a new version
  // of it to install.
  UpdateInstallShim(std::string extension_id,
                    const base::FilePath& extension_root,
                    const UpdateInstallShimCallback& callback);

  // Called when an update attempt failed.
  void OnUpdateError(int error) override;

  // This is called when a new version of an extension is unpacked at
  // |unpack_path| and is ready for install.
  bool Install(const base::DictionaryValue& manifest,
               const base::FilePath& unpack_path) override;

  // This is called by the generic differential update code in the
  // update_client to provide the path to an existing file in the current
  // version of the extension, so that it can be copied (or serve as the input
  // to diff-patching) with output going to the directory with the new version
  // being staged on disk for install.
  bool GetInstalledFile(const std::string& file,
                        base::FilePath* installed_file) override;

  // This method is not relevant to extension updating.
  bool Uninstall() override;

 private:
  friend class base::RefCountedThreadSafe<UpdateInstallShim>;
  ~UpdateInstallShim() override;

  // Takes ownership of the directory at path |temp_dir|.
  void RunCallbackOnUIThread(const base::FilePath& temp_dir);

  std::string extension_id_;
  base::FilePath extension_root_;
  UpdateInstallShimCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(UpdateInstallShim);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UPDATER_UPDATE_INSTALL_SHIM_H_
