// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WTF_CrashLogging_h
#define WTF_CrashLogging_h

#include "base/debug/crash_logging.h"
#include "wtf/WTFExport.h"

namespace WTF {
namespace debug {

using ScopedCrashKey = base::debug::ScopedCrashKey;

}  // namespace debug
}  // namespace WTF

#endif  // WTF_CrashLogging_h
