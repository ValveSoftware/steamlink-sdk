// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/crash/content/app/breakpad_mac.h"

#import <Foundation/Foundation.h>

// Stubbed out versions of breakpad integration functions so we can compile
// without linking in Breakpad.

namespace breakpad {

bool IsCrashReporterEnabled() {
  return false;
}

void InitCrashProcessInfo(const std::string& process_type_switch) {
}

void InitCrashReporter(const std::string& process_type) {
}

}  // namespace breakpad
