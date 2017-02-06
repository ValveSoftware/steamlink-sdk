// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ELF_BREAKPAD_H_
#define CHROME_ELF_BREAKPAD_H_

#include <windows.h>

namespace google_breakpad {
class ExceptionHandler;
}

// Initializes collection and upload of crash reports. This will only be done if
// the user has agreed to crash dump reporting.
//
// Crash reporting has to be initialized as early as possible (e.g., the first
// thing in main()) to catch crashes occuring during process startup.
// Crashes which occur during the global static construction phase will not
// be caught and reported. This should not be a problem as static non-POD
// objects are not allowed by the style guide and exceptions to this rule are
// rare.
void InitializeCrashReporting();

// Generates a crashdump for the provided |exinfo|. This crashdump will be
// either be saved locally, or uploaded, depending on how the ExceptionHandler
// has been configured.
int GenerateCrashDump(EXCEPTION_POINTERS* exinfo);

// Global pointer to the ExceptionHandler. This is initialized by
// InitializeCrashReporting() and used by GenerateCrashDump() to record dumps.
extern google_breakpad::ExceptionHandler* g_elf_breakpad;

#endif  // CHROME_ELF_BREAKPAD_H_
