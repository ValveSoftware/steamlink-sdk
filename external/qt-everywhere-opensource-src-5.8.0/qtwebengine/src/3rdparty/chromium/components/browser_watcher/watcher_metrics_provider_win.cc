// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/watcher_metrics_provider_win.h"

#include <stddef.h>

#include <limits>
#include <vector>

#include "base/bind.h"
#include "base/metrics/sparse_histogram.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"

namespace browser_watcher {

namespace {

// Process ID APIs on Windows talk in DWORDs, whereas for string formatting
// and parsing, this code uses int. In practice there are no process IDs with
// the high bit set on Windows, so there's no danger of overflow if this is
// done consistently.
static_assert(sizeof(DWORD) == sizeof(int),
              "process ids are expected to be no larger than int");

// This function does soft matching on the PID recorded in the key only.
// Due to PID reuse, the possibility exists that the process that's now live
// with the given PID is not the same process the data was recorded for.
// This doesn't matter for the purpose, as eventually the data will be
// scavenged and reported.
bool IsDeadProcess(base::StringPiece16 key_or_value_name) {
  // Truncate the input string to the first occurrence of '-', if one exists.
  size_t num_end = key_or_value_name.find(L'-');
  if (num_end != base::StringPiece16::npos)
    key_or_value_name = key_or_value_name.substr(0, num_end);

  // Convert to the numeric PID.
  int pid = 0;
  if (!base::StringToInt(key_or_value_name, &pid) || pid == 0)
    return true;

  // This is a very inexpensive check for the common case of our own PID.
  if (static_cast<base::ProcessId>(pid) == base::GetCurrentProcId())
    return false;

  // The process is not our own - see whether a process with this PID exists.
  // This is more expensive than the above check, but should also be very rare,
  // as this only happens more than once for a given PID if a user is running
  // multiple Chrome instances concurrently.
  base::Process process =
      base::Process::Open(static_cast<base::ProcessId>(pid));
  if (process.IsValid()) {
    // The fact that it was possible to open the process says it's live.
    return false;
  }

  return true;
}

void RecordExitCodes(const base::string16& registry_path) {
  base::win::RegKey regkey(HKEY_CURRENT_USER,
                           registry_path.c_str(),
                           KEY_QUERY_VALUE | KEY_SET_VALUE);
  if (!regkey.Valid())
    return;

  size_t num = regkey.GetValueCount();
  if (num == 0)
    return;
  std::vector<base::string16> to_delete;

  // Record the exit codes in a sparse stability histogram, as the range of
  // values used to report failures is large.
  base::HistogramBase* exit_code_histogram =
      base::SparseHistogram::FactoryGet(
          WatcherMetricsProviderWin::kBrowserExitCodeHistogramName,
          base::HistogramBase::kUmaStabilityHistogramFlag);

  for (size_t i = 0; i < num; ++i) {
    base::string16 name;
    if (regkey.GetValueNameAt(static_cast<int>(i), &name) == ERROR_SUCCESS) {
      DWORD exit_code = 0;
      if (regkey.ReadValueDW(name.c_str(), &exit_code) == ERROR_SUCCESS) {
        // Do not report exit codes for processes that are still live,
        // notably for our own process.
        if (exit_code != STILL_ACTIVE || IsDeadProcess(name)) {
          to_delete.push_back(name);
          exit_code_histogram->Add(exit_code);
        }
      }
    }
  }

  // Delete the values reported above.
  for (size_t i = 0; i < to_delete.size(); ++i)
    regkey.DeleteValue(to_delete[i].c_str());
}

void DeleteAllValues(base::win::RegKey* key) {
  DCHECK(key);

  while (key->GetValueCount() != 0) {
    base::string16 value_name;
    LONG res = key->GetValueNameAt(0, &value_name);
    if (res != ERROR_SUCCESS) {
      DVLOG(1) << "Failed to get value name " << res;
      return;
    }

    res = key->DeleteValue(value_name.c_str());
    if (res != ERROR_SUCCESS) {
      DVLOG(1) << "Failed to delete value " << value_name;
      return;
    }
  }
}

void DeleteExitFunnels(const base::string16& registry_path) {
  base::win::RegistryKeyIterator it(HKEY_CURRENT_USER, registry_path.c_str());
  if (!it.Valid())
    return;

  // Exit early if no work to do.
  if (it.SubkeyCount() == 0)
    return;

  // Open the key we use for deletion preemptively to prevent reporting
  // multiple times on permission problems.
  base::win::RegKey key(HKEY_CURRENT_USER,
                        registry_path.c_str(),
                        KEY_QUERY_VALUE);
  if (!key.Valid()) {
    DVLOG(1) << "Failed to open " << registry_path << " for writing.";
    return;
  }

  // Key names to delete.
  std::vector<base::string16> keys_to_delete;
  // Constrain the cleanup to 100 exit funnels at a time, as otherwise this may
  // take a long time to finish where a lot of data has accrued. This will be
  // the case in particular for non-UMA users, as the exit funnel data will
  // accrue without bounds for those users.
  const size_t kMaxCleanup = 100;
  for (; it.Valid() && keys_to_delete.size() < kMaxCleanup; ++it) {
    base::win::RegKey sub_key;
    LONG res =
        sub_key.Open(key.Handle(), it.Name(), KEY_QUERY_VALUE | KEY_SET_VALUE);
    if (res != ERROR_SUCCESS) {
      DVLOG(1) << "Failed to open subkey " << it.Name();
      return;
    }
    DeleteAllValues(&sub_key);

    // Schedule the subkey for deletion.
    keys_to_delete.push_back(it.Name());
  }

  for (const base::string16& key_name : keys_to_delete) {
    LONG res = key.DeleteEmptyKey(key_name.c_str());
    if (res != ERROR_SUCCESS)
      DVLOG(1) << "Failed to delete key " << key_name;
  }
}

// Called from the blocking pool when metrics reporting is disabled, as there
// may be a sizable stash of data to delete.
void DeleteExitCodeRegistryKey(const base::string16& registry_path) {
  CHECK_NE(L"", registry_path);

  DeleteExitFunnels(registry_path);

  base::win::RegKey key;
  LONG res = key.Open(HKEY_CURRENT_USER, registry_path.c_str(),
                      KEY_QUERY_VALUE | KEY_SET_VALUE);
  if (res == ERROR_SUCCESS) {
    DeleteAllValues(&key);
    res = key.DeleteEmptyKey(L"");
  }
  if (res != ERROR_FILE_NOT_FOUND && res != ERROR_SUCCESS)
    DVLOG(1) << "Failed to delete exit code key " << registry_path;
}

}  // namespace

const char WatcherMetricsProviderWin::kBrowserExitCodeHistogramName[] =
    "Stability.BrowserExitCodes";

WatcherMetricsProviderWin::WatcherMetricsProviderWin(
    const base::string16& registry_path,
    base::TaskRunner* cleanup_io_task_runner)
    : recording_enabled_(false),
      cleanup_scheduled_(false),
      registry_path_(registry_path),
      cleanup_io_task_runner_(cleanup_io_task_runner) {
  DCHECK(cleanup_io_task_runner_);
}

WatcherMetricsProviderWin::~WatcherMetricsProviderWin() {
}

void WatcherMetricsProviderWin::OnRecordingEnabled() {
  recording_enabled_ = true;
}

void WatcherMetricsProviderWin::OnRecordingDisabled() {
  if (!recording_enabled_ && !cleanup_scheduled_) {
    // When metrics reporting is disabled, the providers get an
    // OnRecordingDisabled notification at startup. Use that first notification
    // to issue the cleanup task.
    cleanup_io_task_runner_->PostTask(
        FROM_HERE, base::Bind(&DeleteExitCodeRegistryKey, registry_path_));

    cleanup_scheduled_ = true;
  }
}

void WatcherMetricsProviderWin::ProvideStabilityMetrics(
    metrics::SystemProfileProto* /* system_profile_proto */) {
  // Note that if there are multiple instances of Chrome running in the same
  // user account, there's a small race that will double-report the exit codes
  // from both/multiple instances. This ought to be vanishingly rare and will
  // only manifest as low-level "random" noise. To work around this it would be
  // necessary to implement some form of global locking, which is not worth it
  // here.
  RecordExitCodes(registry_path_);
  DeleteExitFunnels(registry_path_);
}

}  // namespace browser_watcher
