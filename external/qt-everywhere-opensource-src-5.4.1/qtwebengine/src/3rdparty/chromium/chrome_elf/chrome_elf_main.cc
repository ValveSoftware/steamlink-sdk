// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "chrome_elf/chrome_elf_main.h"

#include "chrome_elf/blacklist/blacklist.h"
#include "chrome_elf/breakpad.h"
#include "chrome_elf/ntdll_cache.h"

void SignalChromeElf() {
  blacklist::ResetBeacon();
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
  if (reason == DLL_PROCESS_ATTACH) {
    InitializeCrashReporting();

    __try {
      InitCache();
      blacklist::Initialize(false);  // Don't force, abort if beacon is present.
    } __except(GenerateCrashDump(GetExceptionInformation())) {
    }
  }

  return TRUE;
}
