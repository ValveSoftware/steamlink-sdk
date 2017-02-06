// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CRASH_LINUX_SYNCHRONIZED_MINIDUMP_MANAGER_H_
#define CHROMECAST_CRASH_LINUX_SYNCHRONIZED_MINIDUMP_MANAGER_H_

#include <time.h>

#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/values.h"
#include "chromecast/crash/linux/dump_info.h"

namespace chromecast {

// Abstract base class for mutually-exclusive minidump handling. Ensures
// synchronized access among instances of this class to the minidumps directory
// using a file lock. The "lockfile" also holds serialized metadata about each
// of the minidumps in the directory. Derived classes should not access the
// lockfile directly. Instead, use protected methods to query and modify the
// metadata, but only within the implementation of DoWork().
//
// This class holds an in memory representation of the lockfile and metadata
// file when the lockfile is held. Modifier methods work on this in memory
// representation. When the lockfile is released, the in memory representations
// are written to file
//
// The lockfile file is of the following format:
// { <dump_info1> }
// { <dump_info2> }
// ...
// { <dump_infoN> }
//
// Note that this isn't a valid json object. It is formatted in this way so
// that producers to this file do not need to understand json.
//
// Current external producers:
// + watchdog
//
//
// The metadata file is a separate file containing a json dictionary.
//
class SynchronizedMinidumpManager {
 public:
  // Length of a ratelimit period in seconds.
  static const int kRatelimitPeriodSeconds;

  // Number of dumps allowed per period.
  static const int kRatelimitPeriodMaxDumps;

  virtual ~SynchronizedMinidumpManager();

  // Returns whether this object's file locking method is nonblocking or not.
  bool non_blocking() { return non_blocking_; }

  // Sets the file locking mechansim to be nonblocking or not.
  void set_non_blocking(bool non_blocking) { non_blocking_ = non_blocking; }

 protected:
  SynchronizedMinidumpManager();

  // Acquires the lock, calls DoWork(), then releases the lock when DoWork()
  // returns. Derived classes should expose a method which calls this. Returns
  // the status of DoWork(), or -1 if the lock was not successfully acquired.
  int AcquireLockAndDoWork();

  // Derived classes must implement this method. It will be called from
  // DoWorkLocked after the lock has been successfully acquired. The lockfile
  // shall be accessed and mutated only through the methods below. All other
  // files shall be managed as needed by the derived class.
  virtual int DoWork() = 0;

  // Get the current dumps in the lockfile.
  ScopedVector<DumpInfo> GetDumps();

  // Set |dumps| as the dumps in |lockfile_|, replacing current list of dumps.
  int SetCurrentDumps(const ScopedVector<DumpInfo>& dumps);

  // Serialize |dump_info| and append it to the lockfile. Note that the child
  // class must only call this inside DoWork(). This should be the only method
  // used to write to the lockfile. Only call this if the minidump has been
  // generated in the minidumps directory successfully. Returns 0 on success,
  // -1 otherwise.
  int AddEntryToLockFile(const DumpInfo& dump_info);

  // Remove the lockfile entry at |index| in the current in memory
  // representation of the lockfile. If the index is invalid returns -1.
  // Otherwise returns 0.
  int RemoveEntryFromLockFile(int index);

  // Get the number of un-uploaded dumps in the dump_path directory.
  // If delete_all_dumps is true, also delete all these files, this is used to
  // clean lingering dump files.
  int GetNumDumps(bool delete_all_dumps);

  // Increment the number of dumps in the current ratelimit period.
  // Returns 0 on success, < 0 on error.
  int IncrementNumDumpsInCurrentPeriod();

  // Returns true when dumps uploaded in current rate limit period is less than
  // |kRatelimitPeriodMaxDumps|. Resets rate limit period if period time has
  // elapsed.
  bool CanUploadDump();

  // Returns true when there are dumps in the lockfile or extra files in the
  // dump directory, false otherwise.
  // Used to avoid unnecessary file locks in consumers.
  bool HasDumps();

  // If true, the flock on the lockfile will be nonblocking.
  bool non_blocking_;

  // Cached path for the minidumps directory.
  const base::FilePath dump_path_;

 private:
  // Acquire the lock file. Blocks if another process holds it, or if called
  // a second time by the same process. Returns the fd of the lockfile if
  // successful, or -1 if failed.
  int AcquireLockFile();

  // Parse the lockfile and metadata file, populating |dumps_| and |metadata_|
  // for modifier functions to use. Return -1 if an error occurred. Otherwise,
  // return 0. This must not be called unless |this| has acquired the lock.
  int ParseFiles();

  // Write deserialized |dumps| to |lockfile_path_| and the deserialized
  // |metadata| to |metadata_path_|.
  int WriteFiles(const base::ListValue* dumps, const base::Value* metadata);

  // Creates an empty lock file and an initialized metadata file.
  int InitializeFiles();

  // Release the lock file with the associated *fd*.
  void ReleaseLockFile();

  const std::string lockfile_path_;
  const std::string metadata_path_;
  int lockfile_fd_;
  std::unique_ptr<base::Value> metadata_;
  std::unique_ptr<base::ListValue> dumps_;

  DISALLOW_COPY_AND_ASSIGN(SynchronizedMinidumpManager);
};

}  // namespace chromecast

#endif  // CHROMECAST_CRASH_LINUX_SYNCHRONIZED_MINIDUMP_MANAGER_H_
