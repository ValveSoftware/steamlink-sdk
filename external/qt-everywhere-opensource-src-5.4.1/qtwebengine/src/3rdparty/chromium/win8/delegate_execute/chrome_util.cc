// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "win8/delegate_execute/chrome_util.h"

#include <windows.h>
#include <atlbase.h>
#include <shlobj.h>

#include <algorithm>
#include <limits>
#include <string>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/md5.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process_handle.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/util_constants.h"
#include "google_update/google_update_idl.h"

namespace {

#if defined(GOOGLE_CHROME_BUILD)

// TODO(grt): These constants live in installer_util.  Consider moving them
// into common_constants to allow for reuse.
const base::FilePath::CharType kNewChromeExe[] =
    FILE_PATH_LITERAL("new_chrome.exe");
const wchar_t kRenameCommandValue[] = L"cmd";
const wchar_t kRegPathChromeClientBase[] =
    L"Software\\Google\\Update\\Clients\\";

// Returns the name of the global event used to detect if |chrome_exe| is in
// use by a browser process.
// TODO(grt): Move this somewhere central so it can be used by both this
// IsBrowserRunning (below) and IsBrowserAlreadyRunning (browser_util_win.cc).
base::string16 GetEventName(const base::FilePath& chrome_exe) {
  static wchar_t const kEventPrefix[] = L"Global\\";
  const size_t prefix_len = arraysize(kEventPrefix) - 1;
  base::string16 name;
  name.reserve(prefix_len + chrome_exe.value().size());
  name.assign(kEventPrefix, prefix_len);
  name.append(chrome_exe.value());
  std::replace(name.begin() + prefix_len, name.end(), '\\', '!');
  std::transform(name.begin() + prefix_len, name.end(),
                 name.begin() + prefix_len, tolower);
  return name;
}

// Returns true if |chrome_exe| is in use by a browser process. In this case,
// "in use" means past ChromeBrowserMainParts::PreMainMessageLoopRunImpl.
bool IsBrowserRunning(const base::FilePath& chrome_exe) {
  base::win::ScopedHandle handle(::OpenEvent(
      SYNCHRONIZE, FALSE, GetEventName(chrome_exe).c_str()));
  if (handle.IsValid())
    return true;
  DWORD last_error = ::GetLastError();
  if (last_error != ERROR_FILE_NOT_FOUND) {
    AtlTrace("%hs. Failed to open browser event; error %u.\n", __FUNCTION__,
             last_error);
  }
  return false;
}

// Returns true if the file new_chrome.exe exists in the same directory as
// |chrome_exe|.
bool NewChromeExeExists(const base::FilePath& chrome_exe) {
  base::FilePath new_chrome_exe(chrome_exe.DirName().Append(kNewChromeExe));
  return base::PathExists(new_chrome_exe);
}

bool GetUpdateCommand(bool is_per_user, base::string16* update_command) {
  const HKEY root = is_per_user ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  base::string16 reg_path_chrome_client = kRegPathChromeClientBase;
  reg_path_chrome_client.append(dist->GetAppGuid());
  base::win::RegKey key(root, reg_path_chrome_client.c_str(), KEY_QUERY_VALUE);

  return key.ReadValue(kRenameCommandValue, update_command) == ERROR_SUCCESS;
}

#endif  // GOOGLE_CHROME_BUILD

}  // namespace

namespace delegate_execute {

void UpdateChromeIfNeeded(const base::FilePath& chrome_exe) {
#if defined(GOOGLE_CHROME_BUILD)
  // Nothing to do if a browser is already running or if there's no
  // new_chrome.exe.
  if (IsBrowserRunning(chrome_exe) || !NewChromeExeExists(chrome_exe))
    return;

  base::win::ScopedHandle process_handle;

  if (InstallUtil::IsPerUserInstall(chrome_exe.value().c_str())) {
    // Read the update command from the registry.
    base::string16 update_command;
    if (!GetUpdateCommand(true, &update_command)) {
      AtlTrace("%hs. Failed to read update command from registry.\n",
               __FUNCTION__);
    } else {
      // Run the update command.
      base::LaunchOptions launch_options;
      launch_options.start_hidden = true;
      if (!base::LaunchProcess(update_command, launch_options,
                               &process_handle)) {
        AtlTrace("%hs. Failed to launch command to finalize update; "
                 "error %u.\n", __FUNCTION__, ::GetLastError());
      }
    }
  } else {
    // Run the update command via Google Update.
    HRESULT hr = S_OK;
    base::win::ScopedComPtr<IProcessLauncher> process_launcher;
    hr = process_launcher.CreateInstance(__uuidof(ProcessLauncherClass));
    if (FAILED(hr)) {
      AtlTrace("%hs. Failed to Create ProcessLauncher; hr=0x%X.\n",
               __FUNCTION__, hr);
    } else {
      ULONG_PTR handle = 0;
      BrowserDistribution* dist = BrowserDistribution::GetDistribution();
      hr = process_launcher->LaunchCmdElevated(
          dist->GetAppGuid().c_str(), kRenameCommandValue,
          GetCurrentProcessId(), &handle);
      if (FAILED(hr)) {
        AtlTrace("%hs. Failed to launch command to finalize update; "
                 "hr=0x%X.\n", __FUNCTION__, hr);
      } else {
        process_handle.Set(reinterpret_cast<base::ProcessHandle>(handle));
      }
    }
  }

  // Wait for the update to complete and report the results.
  if (process_handle.IsValid()) {
    int exit_code = 0;
    // WaitForExitCode will close the handle in all cases.
    if (!base::WaitForExitCode(process_handle.Take(), &exit_code)) {
      AtlTrace("%hs. Failed to get result when finalizing update.\n",
               __FUNCTION__);
    } else if (exit_code != installer::RENAME_SUCCESSFUL) {
      AtlTrace("%hs. Failed to finalize update with exit code %d.\n",
               __FUNCTION__, exit_code);
    } else {
      AtlTrace("%hs. Finalized pending update.\n", __FUNCTION__);
    }
  }
#endif
}

}  // delegate_execute
