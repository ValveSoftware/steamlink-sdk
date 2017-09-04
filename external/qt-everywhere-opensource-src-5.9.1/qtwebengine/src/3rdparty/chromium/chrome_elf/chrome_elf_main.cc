// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_elf/chrome_elf_main.h"

#include <assert.h>
#include <windows.h>

#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_util.h"
#include "chrome/install_static/product_install_details.h"
#include "chrome_elf/blacklist/blacklist.h"
#include "chrome_elf/crash/crash_helper.h"

// This function is a temporary workaround for https://crbug.com/655788. We
// need to come up with a better way to initialize crash reporting that can
// happen inside DllMain().
void SignalInitializeCrashReporting() {
  if (!elf_crash::InitializeCrashReporting()) {
#ifdef _DEBUG
    assert(false);
#endif  // _DEBUG
  }
}

void SignalChromeElf() {
  blacklist::ResetBeacon();
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
  if (reason == DLL_PROCESS_ATTACH) {
    install_static::InitializeProductDetailsForPrimaryModule();

// CRT on initialization installs an exception filter which calls
// TerminateProcess. We need to hook CRT's attempt to set an exception.
// NOTE: Do not hook if ASan is present, or ASan will fail to install
// its own unhandled exception filter.
#if !defined(ADDRESS_SANITIZER)
    elf_crash::DisableSetUnhandledExceptionFilter();
#endif  // !defined (ADDRESS_SANITIZER)

    install_static::InitializeProcessType();

    __try {
      blacklist::Initialize(false);  // Don't force, abort if beacon is present.
    } __except (elf_crash::GenerateCrashDump(GetExceptionInformation())) {
    }
  } else if (reason == DLL_PROCESS_DETACH) {
    elf_crash::ShutdownCrashReporting();
  }
  return TRUE;
}
