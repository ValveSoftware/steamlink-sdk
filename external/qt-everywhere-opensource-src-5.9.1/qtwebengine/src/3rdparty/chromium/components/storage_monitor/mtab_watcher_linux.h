// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MtabWatcherLinux listens for mount point changes from a mtab file and
// notifies a StorageMonitorLinux about them.
// MtabWatcherLinux lives on the FILE thread.

#ifndef COMPONENTS_STORAGE_MONITOR_MTAB_WATCHER_LINUX_H_
#define COMPONENTS_STORAGE_MONITOR_MTAB_WATCHER_LINUX_H_

#if defined(OS_CHROMEOS)
#error "ChromeOS does not use MtabWatcherLinux."
#endif

#include <map>

#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"

namespace storage_monitor {

class MtabWatcherLinux {
 public:
  // (mount point, mount device)
  // A mapping from mount point to mount device, as extracted from the mtab
  // file.
  typedef std::map<base::FilePath, base::FilePath> MountPointDeviceMap;

  class Delegate {
   public:
    virtual ~Delegate() {}

    // Parses |new_mtab| and find all changes. Called on the UI thread.
    virtual void UpdateMtab(const MountPointDeviceMap& new_mtab) = 0;
  };

  MtabWatcherLinux(const base::FilePath& mtab_path,
                   base::WeakPtr<Delegate> delegate);
  ~MtabWatcherLinux();

 private:
  // Reads mtab file entries into |mtab|.
  void ReadMtab() const;

  // Called when |mtab_path_| changes.
  void OnFilePathChanged(const base::FilePath& path, bool error);

  // Mtab file that lists the mount points.
  const base::FilePath mtab_path_;

  // Watcher for |mtab_path_|.
  base::FilePathWatcher file_watcher_;

  base::WeakPtr<Delegate> delegate_;

  base::WeakPtrFactory<MtabWatcherLinux> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MtabWatcherLinux);
};

}  // namespace storage_monitor

#endif  // COMPONENTS_STORAGE_MONITOR_MTAB_WATCHER_LINUX_H_
