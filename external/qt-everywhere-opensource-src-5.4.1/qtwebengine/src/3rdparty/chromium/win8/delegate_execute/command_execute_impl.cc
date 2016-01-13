// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Implementation of the CommandExecuteImpl class which implements the
// IExecuteCommand and related interfaces for handling ShellExecute based
// launches of the Chrome browser.

#include "win8/delegate_execute/command_execute_impl.h"

#include <shlguid.h>

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process_handle.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/message_window.h"
#include "base/win/registry.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_handle.h"
#include "base/win/scoped_process_information.h"
#include "base/win/win_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "ui/base/clipboard/clipboard_util_win.h"
#include "ui/gfx/win/dpi.h"
#include "win8/delegate_execute/chrome_util.h"
#include "win8/delegate_execute/delegate_execute_util.h"
#include "win8/viewer/metro_viewer_constants.h"

namespace {
// Helper function to retrieve the url from IShellItem interface passed in.
// Returns S_OK on success.
HRESULT GetUrlFromShellItem(IShellItem* shell_item, base::string16* url) {
  DCHECK(shell_item);
  DCHECK(url);
  // First attempt to get the url from the underlying IDataObject if any. This
  // ensures that we get the full url, i.e. including the anchor.
  // If we fail to get the underlying IDataObject we retrieve the url via the
  // IShellItem::GetDisplayName function.
  CComPtr<IDataObject> object;
  HRESULT hr = shell_item->BindToHandler(NULL,
                                         BHID_DataObject,
                                         IID_IDataObject,
                                         reinterpret_cast<void**>(&object));
  if (SUCCEEDED(hr)) {
    DCHECK(object);
    if (ui::ClipboardUtil::GetPlainText(object, url))
      return S_OK;
  }

  base::win::ScopedCoMem<wchar_t> name;
  hr = shell_item->GetDisplayName(SIGDN_URL, &name);
  if (hr != S_OK) {
    AtlTrace("Failed to get display name\n");
    return hr;
  }

  *url = static_cast<const wchar_t*>(name);
  AtlTrace("Retrieved url from display name %ls\n", url->c_str());
  return S_OK;
}

bool LaunchChromeBrowserProcess() {
  base::FilePath delegate_exe_path;
  if (!PathService::Get(base::FILE_EXE, &delegate_exe_path))
    return false;

  // First try and go up a level to find chrome.exe.
  base::FilePath chrome_exe_path =
      delegate_exe_path.DirName()
                       .DirName()
                       .Append(chrome::kBrowserProcessExecutableName);
  if (!base::PathExists(chrome_exe_path)) {
    // Try looking in the current directory if we couldn't find it one up in
    // order to support developer installs.
    chrome_exe_path =
        delegate_exe_path.DirName()
                         .Append(chrome::kBrowserProcessExecutableName);
  }

  if (!base::PathExists(chrome_exe_path)) {
    AtlTrace("Could not locate chrome.exe at: %ls\n",
             chrome_exe_path.value().c_str());
    return false;
  }

  CommandLine cl(chrome_exe_path);

  // Prevent a Chrome window from showing up on the desktop.
  cl.AppendSwitch(switches::kSilentLaunch);

  // Tell Chrome to connect to the Metro viewer process.
  cl.AppendSwitch(switches::kViewerConnect);

  base::LaunchOptions launch_options;
  launch_options.start_hidden = true;

  return base::LaunchProcess(cl, launch_options, NULL);
}

}  // namespace

bool CommandExecuteImpl::path_provider_initialized_ = false;

// CommandExecuteImpl is responsible for activating chrome in Windows 8. The
// flow is complicated and this tries to highlight the important events.
// The current approach is to have a single instance of chrome either
// running in desktop or metro mode. If there is no current instance then
// the desktop shortcut launches desktop chrome and the metro tile or search
// charm launches metro chrome.
// If chrome is running then focus/activation is given to the existing one
// regardless of what launch point the user used.
//
// The general flow for activation is as follows:
//
// 1- User interacts with launch point (icon, tile, search, shellexec, etc)
// 2- Windows finds the appid for launch item and resolves it to chrome
// 3- Windows activates CommandExecuteImpl inside a surrogate process
// 4- Windows calls the following sequence of entry points:
//    CommandExecuteImpl::SetShowWindow
//    CommandExecuteImpl::SetPosition
//    CommandExecuteImpl::SetDirectory
//    CommandExecuteImpl::SetParameter
//    CommandExecuteImpl::SetNoShowUI
//    CommandExecuteImpl::SetSelection
//    CommandExecuteImpl::Initialize
//    Up to this point the code basically just gathers values passed in, like
//    the launch scheme (or url) and the activation verb.
// 5- Windows calls CommandExecuteImpl::Getvalue()
//    Here we need to return AHE_IMMERSIVE or AHE_DESKTOP. That depends on:
//    a) if run in high-integrity return AHE_DESKTOP.
//    b) else we return what GetLaunchMode() tells us, which is:
//       i) if chrome is not the default browser, return AHE_DESKTOP
//       ii) if the command line --force-xxx is present return that
//       iii) if the registry 'launch_mode' exists return that
//       iv) else return AHE_DESKTOP
// 6- If we returned AHE_IMMERSIVE in step 5 windows might not call us back
//    and simply activate chrome in metro by itself, however in some cases
//    it might proceed at step 7.
//    As far as we know if we return AHE_DESKTOP then step 7 always happens.
// 7- Windows calls CommandExecuteImpl::Execute()
//    Here we call GetLaunchMode() which returns the cached answer
//    computed at step 5c. which can be:
//    a) ECHUIM_DESKTOP then we call LaunchDesktopChrome() that calls
//       ::CreateProcess and we exit at this point even on failure.
//    b) else we call one of the IApplicationActivationManager activation
//       functions depending on the parameters passed in step 4.
//    c) If the activation returns E_APPLICATION_NOT_REGISTERED, then we fall
//       back to launching chrome on the desktop via LaunchDestopChrome().  Note
//       that this case can lead to strange behavior, because at this point we
//       have pre-launched the browser with --silent-launch --viewer-connect.
//       E_APPLICATION_NOT_REGISTERED is always returned if Chrome is not the
//       default browser (this case will have already been checked for by
//       GetLaunchMode() and AHE_DESKTOP returned), but we don't know if it can
//       be returned for other reasons.
//
// Note that if a command line --force-xxx is present we write that launch mode
// in the registry so next time the logic reaches 5c-ii it will use the same
// mode again.
//
CommandExecuteImpl::CommandExecuteImpl()
    : parameters_(CommandLine::NO_PROGRAM),
      launch_scheme_(INTERNET_SCHEME_DEFAULT),
      integrity_level_(base::INTEGRITY_UNKNOWN) {
  memset(&start_info_, 0, sizeof(start_info_));
  start_info_.cb = sizeof(start_info_);

  // We need to query the user data dir of chrome so we need chrome's
  // path provider. We can be created multiple times in a single instance
  // however so make sure we do this only once.
  if (!path_provider_initialized_) {
    chrome::RegisterPathProvider();
    path_provider_initialized_ = true;
  }
}

// CommandExecuteImpl
STDMETHODIMP CommandExecuteImpl::SetKeyState(DWORD key_state) {
  return S_OK;
}

STDMETHODIMP CommandExecuteImpl::SetParameters(LPCWSTR params) {
  parameters_ = delegate_execute::CommandLineFromParameters(params);
  return S_OK;
}

STDMETHODIMP CommandExecuteImpl::SetPosition(POINT pt) {
  return S_OK;
}

STDMETHODIMP CommandExecuteImpl::SetShowWindow(int show) {
  start_info_.wShowWindow = show;
  start_info_.dwFlags |= STARTF_USESHOWWINDOW;
  return S_OK;
}

STDMETHODIMP CommandExecuteImpl::SetNoShowUI(BOOL no_show_ui) {
  return S_OK;
}

STDMETHODIMP CommandExecuteImpl::SetDirectory(LPCWSTR directory) {
  return S_OK;
}

void CommandExecuteImpl::SetHighDPIRegistryKey(bool enable) {
  uint32 key_value = enable ? 1 : 2;
  base::win::RegKey high_dpi_key(HKEY_CURRENT_USER);
  high_dpi_key.CreateKey(gfx::win::kRegistryProfilePath, KEY_SET_VALUE);
  high_dpi_key.WriteValue(gfx::win::kHighDPISupportW, key_value);
}

STDMETHODIMP CommandExecuteImpl::GetValue(enum AHE_TYPE* pahe) {
  if (!GetLaunchScheme(&display_name_, &launch_scheme_)) {
    AtlTrace("Failed to get scheme, E_FAIL\n");
    return E_FAIL;
  }

  EC_HOST_UI_MODE mode = GetLaunchMode();
  *pahe = (mode == ECHUIM_DESKTOP) ? AHE_DESKTOP : AHE_IMMERSIVE;

  // If we're going to return AHE_IMMERSIVE, then both the browser process and
  // the metro viewer need to launch and connect before the user can start
  // browsing.  However we must not launch the metro viewer until we get a
  // call to CommandExecuteImpl::Execute().  If we wait until then to launch
  // the browser process as well, it will appear laggy while they connect to
  // each other, so we pre-launch the browser process now.
  if (*pahe == AHE_IMMERSIVE && verb_ != win8::kMetroViewerConnectVerb) {
    SetHighDPIRegistryKey(true);
    LaunchChromeBrowserProcess();
  }
  return S_OK;
}

STDMETHODIMP CommandExecuteImpl::Execute() {
  AtlTrace("In %hs\n", __FUNCTION__);

  if (integrity_level_ == base::HIGH_INTEGRITY)
    return LaunchDesktopChrome();

  EC_HOST_UI_MODE mode = GetLaunchMode();
  if (mode == ECHUIM_DESKTOP)
    return LaunchDesktopChrome();

  HRESULT hr = E_FAIL;
  CComPtr<IApplicationActivationManager> activation_manager;
  hr = activation_manager.CoCreateInstance(CLSID_ApplicationActivationManager);
  if (!activation_manager) {
    AtlTrace("Failed to get the activation manager, error 0x%x\n", hr);
    return S_OK;
  }

  BrowserDistribution* distribution = BrowserDistribution::GetDistribution();
  bool is_per_user_install = InstallUtil::IsPerUserInstall(
      chrome_exe_.value().c_str());
  base::string16 app_id = ShellUtil::GetBrowserModelId(
      distribution, is_per_user_install);

  DWORD pid = 0;
  if (launch_scheme_ == INTERNET_SCHEME_FILE &&
      display_name_.find(installer::kChromeExe) != base::string16::npos) {
    AtlTrace("Activating for file\n");
    hr = activation_manager->ActivateApplication(app_id.c_str(),
                                                 verb_.c_str(),
                                                 AO_NONE,
                                                 &pid);
  } else {
    AtlTrace("Activating for protocol\n");
    hr = activation_manager->ActivateForProtocol(app_id.c_str(),
                                                 item_array_,
                                                 &pid);
  }
  if (hr == E_APPLICATION_NOT_REGISTERED) {
    AtlTrace("Metro chrome is not registered, launching in desktop\n");
    return LaunchDesktopChrome();
  }
  AtlTrace("Metro Chrome launch, pid=%d, returned 0x%x\n", pid, hr);
  return S_OK;
}

STDMETHODIMP CommandExecuteImpl::Initialize(LPCWSTR name,
                                            IPropertyBag* bag) {
  if (!FindChromeExe(&chrome_exe_))
    return E_FAIL;
  delegate_execute::UpdateChromeIfNeeded(chrome_exe_);

  if (name) {
    AtlTrace("Verb is %S\n", name);
    verb_ = name;
  }

  base::GetProcessIntegrityLevel(base::GetCurrentProcessHandle(),
                                 &integrity_level_);
  return S_OK;
}

STDMETHODIMP CommandExecuteImpl::SetSelection(IShellItemArray* item_array) {
  item_array_ = item_array;
  return S_OK;
}

STDMETHODIMP CommandExecuteImpl::GetSelection(REFIID riid, void** selection) {
  return S_OK;
}

STDMETHODIMP CommandExecuteImpl::AllowForegroundTransfer(void* reserved) {
  return S_OK;
}

// Returns false if chrome.exe cannot be found.
// static
bool CommandExecuteImpl::FindChromeExe(base::FilePath* chrome_exe) {
  // Look for chrome.exe one folder above delegate_execute.exe (as expected in
  // Chrome installs). Failing that, look for it alonside delegate_execute.exe.
  base::FilePath dir_exe;
  if (!PathService::Get(base::DIR_EXE, &dir_exe)) {
    AtlTrace("Failed to get current exe path\n");
    return false;
  }

  *chrome_exe = dir_exe.DirName().Append(chrome::kBrowserProcessExecutableName);
  if (!base::PathExists(*chrome_exe)) {
    *chrome_exe = dir_exe.Append(chrome::kBrowserProcessExecutableName);
    if (!base::PathExists(*chrome_exe)) {
      AtlTrace("Failed to find chrome exe file\n");
      return false;
    }
  }
  return true;
}

bool CommandExecuteImpl::GetLaunchScheme(
    base::string16* display_name, INTERNET_SCHEME* scheme) {
  if (!item_array_)
    return false;

  ATLASSERT(display_name);
  ATLASSERT(scheme);

  DWORD count = 0;
  item_array_->GetCount(&count);

  if (count != 1) {
    AtlTrace("Cannot handle %d elements in the IShellItemArray\n", count);
    return false;
  }

  CComPtr<IEnumShellItems> items;
  item_array_->EnumItems(&items);
  CComPtr<IShellItem> shell_item;
  HRESULT hr = items->Next(1, &shell_item, &count);
  if (hr != S_OK) {
    AtlTrace("Failed to read element from the IShellItemsArray\n");
    return false;
  }

  hr = GetUrlFromShellItem(shell_item, display_name);
  if (FAILED(hr)) {
    AtlTrace("Failed to get url. Error 0x%x\n", hr);
    return false;
  }

  wchar_t scheme_name[16];
  URL_COMPONENTS components = {0};
  components.lpszScheme = scheme_name;
  components.dwSchemeLength = sizeof(scheme_name)/sizeof(scheme_name[0]);

  components.dwStructSize = sizeof(components);
  if (!InternetCrackUrlW(display_name->c_str(), 0, 0, &components)) {
    AtlTrace("Failed to crack url %ls\n", display_name->c_str());
    return false;
  }

  AtlTrace("Launch scheme is [%ls] (%d)\n", scheme_name, components.nScheme);
  *scheme = components.nScheme;
  return true;
}

HRESULT CommandExecuteImpl::LaunchDesktopChrome() {
  base::string16 display_name = display_name_;

  switch (launch_scheme_) {
    case INTERNET_SCHEME_FILE:
      // If anything other than chrome.exe is passed in the display name we
      // should honor it. For e.g. If the user clicks on a html file when
      // chrome is the default we should treat it as a parameter to be passed
      // to chrome.
      if (display_name.find(installer::kChromeExe) != base::string16::npos)
        display_name.clear();
      break;

    default:
      break;
  }

  SetHighDPIRegistryKey(false);

  CommandLine chrome(
      delegate_execute::MakeChromeCommandLine(chrome_exe_, parameters_,
                                              display_name));
  base::string16 command_line(chrome.GetCommandLineString());

  AtlTrace("Formatted command line is %ls\n", command_line.c_str());

  PROCESS_INFORMATION temp_process_info = {};
  BOOL ret = CreateProcess(chrome_exe_.value().c_str(),
                           const_cast<LPWSTR>(command_line.c_str()),
                           NULL, NULL, FALSE, 0, NULL, NULL, &start_info_,
                           &temp_process_info);
  if (ret) {
    base::win::ScopedProcessInformation proc_info(temp_process_info);
    AtlTrace("Process id is %d\n", proc_info.process_id());
    AllowSetForegroundWindow(proc_info.process_id());
  } else {
    AtlTrace("Process launch failed, error %d\n", ::GetLastError());
  }

  return S_OK;
}

EC_HOST_UI_MODE CommandExecuteImpl::GetLaunchMode() {
  // See the header file for an explanation of the mode selection logic.
  static bool launch_mode_determined = false;
  static EC_HOST_UI_MODE launch_mode = ECHUIM_DESKTOP;

  const char* modes[] = { "Desktop", "Immersive", "SysLauncher", "??" };

  if (launch_mode_determined)
    return launch_mode;

  if (integrity_level_ == base::HIGH_INTEGRITY) {
    // Metro mode apps don't work in high integrity mode.
    AtlTrace("High integrity: launching in desktop mode\n");
    launch_mode = ECHUIM_DESKTOP;
    launch_mode_determined = true;
    return launch_mode;
  }

  base::FilePath chrome_exe;
  if (!FindChromeExe(&chrome_exe) ||
      ShellUtil::GetChromeDefaultStateFromPath(chrome_exe) !=
          ShellUtil::IS_DEFAULT) {
    AtlTrace("Chrome is not default: launching in desktop mode\n");
    launch_mode = ECHUIM_DESKTOP;
    launch_mode_determined = true;
    return launch_mode;
  }

  if (GetAsyncKeyState(VK_SHIFT) && GetAsyncKeyState(VK_F11)) {
    AtlTrace("Hotkey: launching in immersive mode\n");
    launch_mode = ECHUIM_IMMERSIVE;
    launch_mode_determined = true;
    return launch_mode;
  }

  // From here on, if we can, we will write the outcome
  // of this function to the registry.
  if (parameters_.HasSwitch(switches::kForceImmersive)) {
    launch_mode = ECHUIM_IMMERSIVE;
    launch_mode_determined = true;
    parameters_ = CommandLine(CommandLine::NO_PROGRAM);
  } else if (parameters_.HasSwitch(switches::kForceDesktop)) {
    launch_mode = ECHUIM_DESKTOP;
    launch_mode_determined = true;
    parameters_ = CommandLine(CommandLine::NO_PROGRAM);
  }

  base::win::RegKey reg_key;
  LONG key_result = reg_key.Create(HKEY_CURRENT_USER,
                                   chrome::kMetroRegistryPath,
                                   KEY_ALL_ACCESS);
  if (key_result != ERROR_SUCCESS) {
    AtlTrace("Failed to open HKCU %ls key, error 0x%x\n",
             chrome::kMetroRegistryPath,
             key_result);
    if (!launch_mode_determined) {
      // If we cannot open the key and we don't know the
      // launch mode we default to desktop mode.
      launch_mode = ECHUIM_DESKTOP;
      launch_mode_determined = true;
    }
    return launch_mode;
  }

  if (launch_mode_determined) {
    AtlTrace("Launch mode forced by cmdline to %s\n", modes[launch_mode]);
    reg_key.WriteValue(chrome::kLaunchModeValue,
                       static_cast<DWORD>(launch_mode));
    return launch_mode;
  }

  // Use the previous mode if available. Else launch in desktop mode.
  DWORD reg_value;
  if (reg_key.ReadValueDW(chrome::kLaunchModeValue,
                          &reg_value) != ERROR_SUCCESS) {
    launch_mode = ECHUIM_DESKTOP;
    AtlTrace("Can't read registry, defaulting to %s\n", modes[launch_mode]);
  } else if (reg_value >= ECHUIM_SYSTEM_LAUNCHER) {
    AtlTrace("Invalid registry launch mode value %u\n", reg_value);
    launch_mode = ECHUIM_DESKTOP;
  } else {
    launch_mode = static_cast<EC_HOST_UI_MODE>(reg_value);
    AtlTrace("Launch mode forced by registry to %s\n", modes[launch_mode]);
  }

  launch_mode_determined = true;
  return launch_mode;
}
