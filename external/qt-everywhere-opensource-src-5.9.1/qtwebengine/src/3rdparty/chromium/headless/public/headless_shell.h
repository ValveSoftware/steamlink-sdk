// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_HEADLESS_SHELL_H_
#define HEADLESS_PUBLIC_HEADLESS_SHELL_H_

#include "headless/public/headless_export.h"

namespace headless {

// Start the Headless Shell application. Intended to be called early in main().
// Returns the exit code for the process.
int HEADLESS_EXPORT HeadlessShellMain(int argc, const char** argv);

}  // namespace headless

#endif  // HEADLESS_PUBLIC_HEADLESS_SHELL_H_
