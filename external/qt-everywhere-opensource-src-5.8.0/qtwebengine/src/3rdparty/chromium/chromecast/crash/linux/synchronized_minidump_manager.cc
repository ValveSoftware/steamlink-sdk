// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/linux/synchronized_minidump_manager.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <utility>

#include "base/files/dir_reader_posix.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "chromecast/base/path_utils.h"
#include "chromecast/base/serializers.h"
#include "chromecast/crash/linux/dump_info.h"

// if |cond| is false, returns |retval|.
#define RCHECK(cond, retval) \
  do {                       \
    if (!(cond)) {           \
      return (retval);       \
    }                        \
  } while (0)

namespace chromecast {

namespace {

const mode_t kDirMode = 0770;
const mode_t kFileMode = 0660;
const char kLockfileName[] = "lockfile";
const char kMetadataName[] = "metadata";
const char kMinidumpsDir[] = "minidumps";

const char kLockfileRatelimitKey[] = "ratelimit";
const char kLockfileRatelimitPeriodStartKey[] = "period_start";
const char kLockfileRatelimitPeriodDumpsKey[] = "period_dumps";
const std::size_t kLockfileNumRatelimitParams = 2;

// Gets the ratelimit parameter dictionary given a deserialized |metadata|.
// Returns nullptr if invalid.
base::DictionaryValue* GetRatelimitParams(base::Value* metadata) {
  base::DictionaryValue* dict;
  base::DictionaryValue* ratelimit_params;
  if (!metadata || !metadata->GetAsDictionary(&dict) ||
      !dict->GetDictionary(kLockfileRatelimitKey, &ratelimit_params)) {
    return nullptr;
  }

  return ratelimit_params;
}

// Returns the time of the current ratelimit period's start in |metadata|.
// Returns (time_t)-1 if an error occurs.
time_t GetRatelimitPeriodStart(base::Value* metadata) {
  time_t result = static_cast<time_t>(-1);
  base::DictionaryValue* ratelimit_params = GetRatelimitParams(metadata);
  RCHECK(ratelimit_params, result);

  std::string period_start_str;
  RCHECK(ratelimit_params->GetString(kLockfileRatelimitPeriodStartKey,
                                     &period_start_str),
         result);

  errno = 0;
  result = static_cast<time_t>(strtoll(period_start_str.c_str(), nullptr, 0));
  if (errno != 0) {
    return static_cast<time_t>(-1);
  }

  return result;
}

// Sets the time of the current ratelimit period's start in |metadata| to
// |period_start|. Returns 0 on success, < 0 on error.
int SetRatelimitPeriodStart(base::Value* metadata, time_t period_start) {
  DCHECK_GE(period_start, 0);

  base::DictionaryValue* ratelimit_params = GetRatelimitParams(metadata);
  RCHECK(ratelimit_params, -1);

  std::string period_start_str =
      base::StringPrintf("%lld", static_cast<long long>(period_start));
  ratelimit_params->SetString(kLockfileRatelimitPeriodStartKey,
                              period_start_str);
  return 0;
}

// Gets the number of dumps added to |metadata| in the current ratelimit
// period. Returns < 0 on error.
int GetRatelimitPeriodDumps(base::Value* metadata) {
  int period_dumps = -1;

  base::DictionaryValue* ratelimit_params = GetRatelimitParams(metadata);
  if (!ratelimit_params ||
      !ratelimit_params->GetInteger(kLockfileRatelimitPeriodDumpsKey,
                                    &period_dumps)) {
    return -1;
  }

  return period_dumps;
}

// Sets the current ratelimit period's number of dumps in |metadata| to
// |period_dumps|. Returns 0 on success, < 0 on error.
int SetRatelimitPeriodDumps(base::Value* metadata, int period_dumps) {
  DCHECK_GE(period_dumps, 0);

  base::DictionaryValue* ratelimit_params = GetRatelimitParams(metadata);
  RCHECK(ratelimit_params, -1);

  ratelimit_params->SetInteger(kLockfileRatelimitPeriodDumpsKey, period_dumps);

  return 0;
}

// Returns true if |metadata| contains valid metadata, false otherwise.
bool ValidateMetadata(base::Value* metadata) {
  RCHECK(metadata, false);

  // Validate ratelimit params
  base::DictionaryValue* ratelimit_params = GetRatelimitParams(metadata);

  return ratelimit_params &&
         ratelimit_params->size() == kLockfileNumRatelimitParams &&
         GetRatelimitPeriodStart(metadata) >= 0 &&
         GetRatelimitPeriodDumps(metadata) >= 0;
}

}  // namespace

// One day
const int SynchronizedMinidumpManager::kRatelimitPeriodSeconds = 24 * 3600;
const int SynchronizedMinidumpManager::kRatelimitPeriodMaxDumps = 100;

SynchronizedMinidumpManager::SynchronizedMinidumpManager()
    : non_blocking_(false),
      dump_path_(GetHomePathASCII(kMinidumpsDir)),
      lockfile_path_(dump_path_.Append(kLockfileName).value()),
      metadata_path_(dump_path_.Append(kMetadataName).value()),
      lockfile_fd_(-1) {
}

SynchronizedMinidumpManager::~SynchronizedMinidumpManager() {
  // Release the lock if held.
  ReleaseLockFile();
}

// TODO(slan): Move some of this pruning logic to ReleaseLockFile?
int SynchronizedMinidumpManager::GetNumDumps(bool delete_all_dumps) {
  DIR* dirp;
  struct dirent* dptr;
  int num_dumps = 0;

  // folder does not exist
  dirp = opendir(dump_path_.value().c_str());
  if (dirp == NULL) {
    LOG(ERROR) << "Unable to open directory " << dump_path_.value();
    return 0;
  }

  while ((dptr = readdir(dirp)) != NULL) {
    struct stat buf;
    const std::string file_fullname = dump_path_.value() + "/" + dptr->d_name;
    if (lstat(file_fullname.c_str(), &buf) == -1 || !S_ISREG(buf.st_mode)) {
      // if we cannot lstat this file, it is probably bad, so skip
      // if the file is not regular, skip
      continue;
    }

    // 'lockfile' and 'metadata' is not counted
    if (lockfile_path_ != file_fullname && metadata_path_ != file_fullname) {
      ++num_dumps;
      if (delete_all_dumps) {
        LOG(INFO) << "Removing " << dptr->d_name
                  << "which was not in the lockfile";
        if (remove(file_fullname.c_str()) < 0) {
          LOG(INFO) << "remove failed. error " << strerror(errno);
        }
      }
    }
  }

  closedir(dirp);
  return num_dumps;
}

int SynchronizedMinidumpManager::AcquireLockAndDoWork() {
  int success = -1;
  if (AcquireLockFile() >= 0) {
    success = DoWork();
    ReleaseLockFile();
  }
  return success;
}

int SynchronizedMinidumpManager::AcquireLockFile() {
  DCHECK_LT(lockfile_fd_, 0);
  // Make the directory for the minidumps if it does not exist.
  if (mkdir(dump_path_.value().c_str(), kDirMode) < 0 && errno != EEXIST) {
    LOG(ERROR) << "mkdir for " << dump_path_.value().c_str()
               << " failed. error = " << strerror(errno);
    return -1;
  }

  // Open the lockfile. Create it if it does not exist.
  lockfile_fd_ = open(lockfile_path_.c_str(), O_RDWR | O_CREAT, kFileMode);

  // If opening or creating the lockfile failed, we don't want to proceed
  // with dump writing for fear of exhausting up system resources.
  if (lockfile_fd_ < 0) {
    LOG(ERROR) << "open lockfile failed " << lockfile_path_;
    return -1;
  }

  // Acquire the lock on the file. Whether or not we are in non-blocking mode,
  // flock failure means that we did not acquire it and this method should fail.
  int operation_mode = non_blocking_ ? (LOCK_EX | LOCK_NB) : LOCK_EX;
  if (flock(lockfile_fd_, operation_mode) < 0) {
    ReleaseLockFile();
    LOG(INFO) << "flock lockfile failed, error = " << strerror(errno);
    return -1;
  }

  // The lockfile is open and locked. Parse it to provide subclasses with a
  // record of all the current dumps.
  if (ParseFiles() < 0) {
    LOG(ERROR) << "Lockfile did not parse correctly. ";
    if (InitializeFiles() < 0 || ParseFiles() < 0) {
      LOG(ERROR) << "Failed to create a new lock file!";
      return -1;
    }
  }

  DCHECK(dumps_);
  DCHECK(metadata_);

  // We successfully have acquired the lock.
  return 0;
}

int SynchronizedMinidumpManager::ParseFiles() {
  DCHECK_GE(lockfile_fd_, 0);
  DCHECK(!dumps_);
  DCHECK(!metadata_);

  std::string lockfile;
  RCHECK(ReadFileToString(base::FilePath(lockfile_path_), &lockfile), -1);

  std::vector<std::string> lines = base::SplitString(
      lockfile, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  std::unique_ptr<base::ListValue> dumps =
      base::WrapUnique(new base::ListValue());

  // Validate dumps
  for (const std::string& line : lines) {
    if (line.size() == 0)
      continue;
    std::unique_ptr<base::Value> dump_info = DeserializeFromJson(line);
    DumpInfo info(dump_info.get());
    RCHECK(info.valid(), -1);
    dumps->Append(std::move(dump_info));
  }

  std::unique_ptr<base::Value> metadata =
      DeserializeJsonFromFile(base::FilePath(metadata_path_));
  RCHECK(ValidateMetadata(metadata.get()), -1);

  dumps_ = std::move(dumps);
  metadata_ = std::move(metadata);
  return 0;
}

int SynchronizedMinidumpManager::WriteFiles(const base::ListValue* dumps,
                                            const base::Value* metadata) {
  DCHECK(dumps);
  DCHECK(metadata);
  std::string lockfile;

  for (const auto& elem : *dumps) {
    std::unique_ptr<std::string> dump_info = SerializeToJson(*elem);
    RCHECK(dump_info, -1);
    lockfile += *dump_info;
    lockfile += "\n";  // Add line seperatators
  }

  if (WriteFile(base::FilePath(lockfile_path_),
                lockfile.c_str(),
                lockfile.size()) < 0) {
    return -1;
  }

  return SerializeJsonToFile(base::FilePath(metadata_path_), *metadata) ? 0
                                                                        : -1;
}

int SynchronizedMinidumpManager::InitializeFiles() {
  std::unique_ptr<base::DictionaryValue> metadata =
      base::WrapUnique(new base::DictionaryValue());

  base::DictionaryValue* ratelimit_fields = new base::DictionaryValue();
  metadata->Set(kLockfileRatelimitKey, base::WrapUnique(ratelimit_fields));
  ratelimit_fields->SetString(kLockfileRatelimitPeriodStartKey, "0");
  ratelimit_fields->SetInteger(kLockfileRatelimitPeriodDumpsKey, 0);

  std::unique_ptr<base::ListValue> dumps =
      base::WrapUnique(new base::ListValue());

  return WriteFiles(dumps.get(), metadata.get());
}

int SynchronizedMinidumpManager::AddEntryToLockFile(const DumpInfo& dump_info) {
  DCHECK_LE(0, lockfile_fd_);
  DCHECK(dumps_);

  // Make sure dump_info is valid.
  if (!dump_info.valid()) {
    LOG(ERROR) << "Entry to be added is invalid";
    return -1;
  }

  dumps_->Append(dump_info.GetAsValue());

  return 0;
}

int SynchronizedMinidumpManager::RemoveEntryFromLockFile(int index) {
  return dumps_->Remove(static_cast<size_t>(index), nullptr) ? 0 : -1;
}

void SynchronizedMinidumpManager::ReleaseLockFile() {
  // flock is associated with the fd entry in the open fd table, so closing
  // all fd's will release the lock. To be safe, we explicitly unlock.
  if (lockfile_fd_ >= 0) {
    if (dumps_)
      WriteFiles(dumps_.get(), metadata_.get());

    flock(lockfile_fd_, LOCK_UN);
    close(lockfile_fd_);

    // We may use this object again, so we should reset this.
    lockfile_fd_ = -1;
  }

  dumps_.reset();
  metadata_.reset();
}

ScopedVector<DumpInfo> SynchronizedMinidumpManager::GetDumps() {
  ScopedVector<DumpInfo> dumps;

  for (const auto& elem : *dumps_) {
    dumps.push_back(new DumpInfo(elem.get()));
  }

  return dumps;
}

int SynchronizedMinidumpManager::SetCurrentDumps(
    const ScopedVector<DumpInfo>& dumps) {
  dumps_->Clear();

  for (DumpInfo* dump : dumps)
    dumps_->Append(dump->GetAsValue());

  return 0;
}

int SynchronizedMinidumpManager::IncrementNumDumpsInCurrentPeriod() {
  DCHECK(metadata_);
  int last_dumps = GetRatelimitPeriodDumps(metadata_.get());
  RCHECK(last_dumps >= 0, -1);

  return SetRatelimitPeriodDumps(metadata_.get(), last_dumps + 1);
}

bool SynchronizedMinidumpManager::CanUploadDump() {
  time_t cur_time = time(nullptr);
  time_t period_start = GetRatelimitPeriodStart(metadata_.get());
  int period_dumps_count = GetRatelimitPeriodDumps(metadata_.get());

  // If we're in invalid state, or we passed the period, reset the ratelimit.
  // When the device reboots, |cur_time| may be incorrectly reported to be a
  // very small number for a short period of time. So only consider
  // |period_start| invalid when |cur_time| is less if |cur_time| is not very
  // close to 0.
  if (period_dumps_count < 0 ||
      (cur_time < period_start && cur_time > kRatelimitPeriodSeconds) ||
      difftime(cur_time, period_start) >= kRatelimitPeriodSeconds) {
    period_start = cur_time;
    period_dumps_count = 0;
    SetRatelimitPeriodStart(metadata_.get(), period_start);
    SetRatelimitPeriodDumps(metadata_.get(), period_dumps_count);
  }

  return period_dumps_count < kRatelimitPeriodMaxDumps;
}

bool SynchronizedMinidumpManager::HasDumps() {
  // Check if lockfile has entries.
  int64_t size = 0;
  if (GetFileSize(base::FilePath(lockfile_path_), &size) && size > 0)
    return true;

  // Check if any files are in minidump directory
  base::DirReaderPosix reader(dump_path_.value().c_str());
  if (!reader.IsValid()) {
    DLOG(FATAL) << "Could not open minidump dir: " << dump_path_.value();
    return false;
  }

  while (reader.Next()) {
    if (strcmp(reader.name(), ".") == 0 || strcmp(reader.name(), "..") == 0)
      continue;

    const std::string file_path = dump_path_.Append(reader.name()).value();
    if (file_path != lockfile_path_ && file_path != metadata_path_)
      return true;
  }

  return false;
}

}  // namespace chromecast
