// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Intentionally no include guards because this file is meant to be included
// inside a macro to generate enum values.

// This file consolidates all the return codes for the browser and renderer
// process. The return code is the value that:
// a) is returned by main() or winmain(), or
// b) specified in the call for ExitProcess() or TerminateProcess(), or
// c) the exception value that causes a process to terminate.
//
// It is advisable to not use negative numbers because the Windows API returns
// it as an unsigned long and the exception values have high numbers. For
// example EXCEPTION_ACCESS_VIOLATION value is 0xC0000005.

#include "build/build_config.h"

// Process terminated normally.
RESULT_CODE(NORMAL_EXIT, 0)

// Process was killed by user or system.
RESULT_CODE(KILLED, 1)

// Process hung.
RESULT_CODE(HUNG, 2)

// A bad message caused the process termination.
RESULT_CODE(KILLED_BAD_MESSAGE, 3)
