// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIN8_DELEGATE_EXECUTE_CHROME_UTIL_H_
#define WIN8_DELEGATE_EXECUTE_CHROME_UTIL_H_

#include "base/strings/string16.h"

namespace base {
class FilePath;
}

namespace delegate_execute {

// Finalizes a previously updated installation.
void UpdateChromeIfNeeded(const base::FilePath& chrome_exe);

}  // namespace delegate_execute

#endif  // WIN8_DELEGATE_EXECUTE_CHROME_UTIL_H_
