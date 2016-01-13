// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_elf/blacklist/blacklist.h"

#include <assert.h>
#include <string.h>

#include <vector>

#include "base/basictypes.h"
#include "chrome_elf/blacklist/blacklist_interceptions.h"
#include "chrome_elf/chrome_elf_constants.h"
#include "chrome_elf/chrome_elf_util.h"
#include "chrome_elf/thunk_getter.h"
#include "sandbox/win/src/interception_internal.h"
#include "sandbox/win/src/internal_types.h"
#include "sandbox/win/src/service_resolver.h"

// http://blogs.msdn.com/oldnewthing/archive/2004/10/25/247180.aspx
extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace blacklist{

// The DLLs listed here are known (or under strong suspicion) of causing crashes
// when they are loaded in the browser. DLLs should only be added to this list
// if there is nothing else Chrome can do to prevent those crashes.
// For more information about how this list is generated, and how to get off
// of it, see:
// https://sites.google.com/a/chromium.org/dev/Home/third-party-developers
const wchar_t* g_troublesome_dlls[kTroublesomeDllsMaxCount] = {
  L"activedetect32.dll",                // Lenovo One Key Theater.
                                        // See crbug.com/379218.
  L"activedetect64.dll",                // Lenovo One Key Theater.
  L"bitguard.dll",                      // Unknown (suspected malware).
  L"cespy.dll",                         // CovenantEyes.
  L"chrmxtn.dll",                       // Unknown (keystroke logger).
  L"cplushook.dll",                     // Unknown (suspected malware).
  L"datamngr.dll",                      // Unknown (suspected adware).
  L"hk.dll",                            // Unknown (keystroke logger).
  L"libapi2hook.dll",                   // V-Bates.
  L"libinject.dll",                     // V-Bates.
  L"libinject2.dll",                    // V-Bates.
  L"libredir2.dll",                     // V-Bates.
  L"libsvn_tsvn32.dll",                 // TortoiseSVN.
  L"libwinhook.dll",                    // V-Bates.
  L"lmrn.dll",                          // Unknown.
  L"minisp.dll",                        // Unknown (suspected malware).
  L"scdetour.dll",                      // Quick Heal Antivirus.
                                        // See crbug.com/382561.
  L"systemk.dll",                       // Unknown (suspected adware).
  L"windowsapihookdll32.dll",           // Lenovo One Key Theater.
                                        // See crbug.com/379218.
  L"windowsapihookdll64.dll",           // Lenovo One Key Theater.
  // Keep this null pointer here to mark the end of the list.
  NULL,
};

bool g_blocked_dlls[kTroublesomeDllsMaxCount] = {};
int g_num_blocked_dlls = 0;

}  // namespace blacklist

// Allocate storage for thunks in a page of this module to save on doing
// an extra allocation at run time.
#pragma section(".crthunk",read,execute)
__declspec(allocate(".crthunk")) sandbox::ThunkData g_thunk_storage;

namespace {

// Record if the blacklist was successfully initialized so processes can easily
// determine if the blacklist is enabled for them.
bool g_blacklist_initialized = false;

// Helper to set DWORD registry values.
DWORD SetDWValue(HKEY* key, const wchar_t* property, DWORD value) {
  return ::RegSetValueEx(*key,
                         property,
                         0,
                         REG_DWORD,
                         reinterpret_cast<LPBYTE>(&value),
                         sizeof(value));
}

bool GenerateStateFromBeaconAndAttemptCount(HKEY* key, DWORD blacklist_state) {
  LONG result = 0;
  if (blacklist_state == blacklist::BLACKLIST_SETUP_RUNNING) {
    // Some part of the blacklist setup failed last time.  If this has occured
    // blacklist::kBeaconMaxAttempts times in a row we switch the state to
    // failed and skip setting up the blacklist.
    DWORD attempt_count = 0;
    DWORD attempt_count_size = sizeof(attempt_count);
    result = ::RegQueryValueEx(*key,
                               blacklist::kBeaconAttemptCount,
                               0,
                               NULL,
                               reinterpret_cast<LPBYTE>(&attempt_count),
                               &attempt_count_size);

    if (result == ERROR_FILE_NOT_FOUND)
      attempt_count = 0;
    else if (result != ERROR_SUCCESS)
      return false;

    ++attempt_count;
    SetDWValue(key, blacklist::kBeaconAttemptCount, attempt_count);

    if (attempt_count >= blacklist::kBeaconMaxAttempts) {
      blacklist_state = blacklist::BLACKLIST_SETUP_FAILED;
      SetDWValue(key, blacklist::kBeaconState, blacklist_state);
      return false;
    }
  } else if (blacklist_state == blacklist::BLACKLIST_ENABLED) {
    // If the blacklist succeeded on the previous run reset the failure
    // counter.
    result =
        SetDWValue(key, blacklist::kBeaconAttemptCount, static_cast<DWORD>(0));
    if (result != ERROR_SUCCESS) {
      return false;
    }
  }
  return true;
}

}  // namespace

namespace blacklist {

#if defined(_WIN64)
  // Allocate storage for the pointer to the old NtMapViewOfSectionFunction.
#pragma section(".oldntmap",write,read)
  __declspec(allocate(".oldntmap"))
    NtMapViewOfSectionFunction g_nt_map_view_of_section_func = NULL;
#endif

bool LeaveSetupBeacon() {
  HKEY key = NULL;
  DWORD disposition = 0;
  LONG result = ::RegCreateKeyEx(HKEY_CURRENT_USER,
                                 kRegistryBeaconPath,
                                 0,
                                 NULL,
                                 REG_OPTION_NON_VOLATILE,
                                 KEY_QUERY_VALUE | KEY_SET_VALUE,
                                 NULL,
                                 &key,
                                 &disposition);
  if (result != ERROR_SUCCESS)
    return false;

  // Retrieve the current blacklist state.
  DWORD blacklist_state = BLACKLIST_STATE_MAX;
  DWORD blacklist_state_size = sizeof(blacklist_state);
  DWORD type = 0;
  result = ::RegQueryValueEx(key,
                             kBeaconState,
                             0,
                             &type,
                             reinterpret_cast<LPBYTE>(&blacklist_state),
                             &blacklist_state_size);

  if (blacklist_state == BLACKLIST_DISABLED || result != ERROR_SUCCESS ||
      type != REG_DWORD) {
    ::RegCloseKey(key);
    return false;
  }

  if (!GenerateStateFromBeaconAndAttemptCount(&key, blacklist_state)) {
    ::RegCloseKey(key);
    return false;
  }

  result = SetDWValue(&key, kBeaconState, BLACKLIST_SETUP_RUNNING);
  ::RegCloseKey(key);

  return (result == ERROR_SUCCESS);
}

bool ResetBeacon() {
  HKEY key = NULL;
  DWORD disposition = 0;
  LONG result = ::RegCreateKeyEx(HKEY_CURRENT_USER,
                                 kRegistryBeaconPath,
                                 0,
                                 NULL,
                                 REG_OPTION_NON_VOLATILE,
                                 KEY_QUERY_VALUE | KEY_SET_VALUE,
                                 NULL,
                                 &key,
                                 &disposition);
  if (result != ERROR_SUCCESS)
    return false;

  DWORD blacklist_state = BLACKLIST_STATE_MAX;
  DWORD blacklist_state_size = sizeof(blacklist_state);
  DWORD type = 0;
  result = ::RegQueryValueEx(key,
                             kBeaconState,
                             0,
                             &type,
                             reinterpret_cast<LPBYTE>(&blacklist_state),
                             &blacklist_state_size);

  if (result != ERROR_SUCCESS || type != REG_DWORD) {
    ::RegCloseKey(key);
    return false;
  }

  // Reaching this point with the setup running state means the setup did not
  // crash, so we reset to enabled.  Any other state indicates that setup was
  // skipped; in that case we leave the state alone for later recording.
  if (blacklist_state == BLACKLIST_SETUP_RUNNING)
    result = SetDWValue(&key, kBeaconState, BLACKLIST_ENABLED);

  ::RegCloseKey(key);
  return (result == ERROR_SUCCESS);
}

int BlacklistSize() {
  int size = -1;
  while (blacklist::g_troublesome_dlls[++size] != NULL) {}

  return size;
}

bool IsBlacklistInitialized() {
  return g_blacklist_initialized;
}

bool AddDllToBlacklist(const wchar_t* dll_name) {
  int blacklist_size = BlacklistSize();
  // We need to leave one space at the end for the null pointer.
  if (blacklist_size + 1 >= kTroublesomeDllsMaxCount)
    return false;
  for (int i = 0; i < blacklist_size; ++i) {
    if (!_wcsicmp(g_troublesome_dlls[i], dll_name))
      return true;
  }

  // Copy string to blacklist.
  wchar_t* str_buffer = new wchar_t[wcslen(dll_name) + 1];
  wcscpy(str_buffer, dll_name);

  g_troublesome_dlls[blacklist_size] = str_buffer;
  g_blocked_dlls[blacklist_size] = false;
  return true;
}

bool RemoveDllFromBlacklist(const wchar_t* dll_name) {
  int blacklist_size = BlacklistSize();
  for (int i = 0; i < blacklist_size; ++i) {
    if (!_wcsicmp(g_troublesome_dlls[i], dll_name)) {
      // Found the thing to remove. Delete it then replace it with the last
      // element.
      delete[] g_troublesome_dlls[i];
      g_troublesome_dlls[i] = g_troublesome_dlls[blacklist_size - 1];
      g_troublesome_dlls[blacklist_size - 1] = NULL;

      // Also update the stats recording if we have blocked this dll or not.
      if (g_blocked_dlls[i])
        --g_num_blocked_dlls;
      g_blocked_dlls[i] = g_blocked_dlls[blacklist_size - 1];
      return true;
    }
  }
  return false;
}

// TODO(csharp): Maybe store these values in the registry so we can
// still report them if Chrome crashes early.
void SuccessfullyBlocked(const wchar_t** blocked_dlls, int* size) {
  if (size == NULL)
    return;

  // If the array isn't valid or big enough, just report the size it needs to
  // be and return.
  if (blocked_dlls == NULL && *size < g_num_blocked_dlls) {
    *size = g_num_blocked_dlls;
    return;
  }

  *size = g_num_blocked_dlls;

  int strings_to_fill = 0;
  for (int i = 0; strings_to_fill < g_num_blocked_dlls && g_troublesome_dlls[i];
       ++i) {
    if (g_blocked_dlls[i]) {
      blocked_dlls[strings_to_fill] = g_troublesome_dlls[i];
      ++strings_to_fill;
    }
  }
}

void BlockedDll(size_t blocked_index) {
  assert(blocked_index < kTroublesomeDllsMaxCount);

  if (!g_blocked_dlls[blocked_index] &&
      blocked_index < kTroublesomeDllsMaxCount) {
    ++g_num_blocked_dlls;
    g_blocked_dlls[blocked_index] = true;
  }
}

bool Initialize(bool force) {
  // Check to see that we found the functions we need in ntdll.
  if (!InitializeInterceptImports())
    return false;

  // Check to see if this is a non-browser process, abort if so.
  if (IsNonBrowserProcess())
    return false;

  // Check to see if the blacklist beacon is still set to running (indicating a
  // failure) or disabled, and abort if so.
  if (!force && !LeaveSetupBeacon())
    return false;

  // It is possible for other dlls to have already patched code by now and
  // attempting to patch their code might result in crashes.
  const bool kRelaxed = false;

  // Create a thunk via the appropriate ServiceResolver instance.
  sandbox::ServiceResolverThunk* thunk = GetThunk(kRelaxed);

  // Don't try blacklisting on unsupported OS versions.
  if (!thunk)
    return false;

  BYTE* thunk_storage = reinterpret_cast<BYTE*>(&g_thunk_storage);

  // Mark the thunk storage as readable and writeable, since we
  // ready to write to it.
  DWORD old_protect = 0;
  if (!VirtualProtect(&g_thunk_storage,
                      sizeof(g_thunk_storage),
                      PAGE_EXECUTE_READWRITE,
                      &old_protect)) {
    return false;
  }

  thunk->AllowLocalPatches();

  // We declare this early so it can be used in the 64-bit block below and
  // still work on 32-bit build when referenced at the end of the function.
  BOOL page_executable = false;

  // Replace the default NtMapViewOfSection with our patched version.
#if defined(_WIN64)
  NTSTATUS ret = thunk->Setup(::GetModuleHandle(sandbox::kNtdllName),
                              reinterpret_cast<void*>(&__ImageBase),
                              "NtMapViewOfSection",
                              NULL,
                              &blacklist::BlNtMapViewOfSection64,
                              thunk_storage,
                              sizeof(sandbox::ThunkData),
                              NULL);

  // Keep a pointer to the original code, we don't have enough space to
  // add it directly to the call.
  g_nt_map_view_of_section_func = reinterpret_cast<NtMapViewOfSectionFunction>(
      thunk_storage);

  // Ensure that the pointer to the old function can't be changed.
 page_executable = VirtualProtect(&g_nt_map_view_of_section_func,
                                  sizeof(g_nt_map_view_of_section_func),
                                  PAGE_EXECUTE_READ,
                                  &old_protect);
#else
  NTSTATUS ret = thunk->Setup(::GetModuleHandle(sandbox::kNtdllName),
                              reinterpret_cast<void*>(&__ImageBase),
                              "NtMapViewOfSection",
                              NULL,
                              &blacklist::BlNtMapViewOfSection,
                              thunk_storage,
                              sizeof(sandbox::ThunkData),
                              NULL);
#endif
  delete thunk;

  // Record if we have initialized the blacklist.
  g_blacklist_initialized = NT_SUCCESS(ret);

  // Mark the thunk storage as executable and prevent any future writes to it.
  page_executable = page_executable && VirtualProtect(&g_thunk_storage,
                                                      sizeof(g_thunk_storage),
                                                      PAGE_EXECUTE_READ,
                                                      &old_protect);

  AddDllsFromRegistryToBlacklist();

  return NT_SUCCESS(ret) && page_executable;
}

bool AddDllsFromRegistryToBlacklist() {
  HKEY key = NULL;
  LONG result = ::RegOpenKeyEx(HKEY_CURRENT_USER,
                               kRegistryFinchListPath,
                               0,
                               KEY_QUERY_VALUE | KEY_SET_VALUE,
                               &key);

  if (result != ERROR_SUCCESS)
    return false;

  // We add dlls from the registry to the blacklist, and then clear registry.
  DWORD value_len;
  DWORD name_len = MAX_PATH;
  std::vector<wchar_t> name_buffer(name_len);
  for (int i = 0; result == ERROR_SUCCESS; ++i) {
    name_len = MAX_PATH;
    value_len = 0;
    result = ::RegEnumValue(
        key, i, &name_buffer[0], &name_len, NULL, NULL, NULL, &value_len);
    name_len = name_len + 1;
    value_len = value_len + 1;
    std::vector<wchar_t> value_buffer(value_len);
    result = ::RegEnumValue(key, i, &name_buffer[0], &name_len, NULL, NULL,
                            reinterpret_cast<BYTE*>(&value_buffer[0]),
                            &value_len);
    value_buffer[value_len - 1] = L'\0';

    if (result == ERROR_SUCCESS) {
      AddDllToBlacklist(&value_buffer[0]);
    }
  }

  // Delete the finch registry key to clear the values.
  result = ::RegDeleteKey(key, L"");

  ::RegCloseKey(key);
  return result == ERROR_SUCCESS;
}

}  // namespace blacklist
