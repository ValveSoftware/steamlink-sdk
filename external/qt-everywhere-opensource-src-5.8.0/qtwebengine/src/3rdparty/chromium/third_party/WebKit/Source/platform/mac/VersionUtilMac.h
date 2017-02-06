// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VersionUtilMac_h
#define VersionUtilMac_h

#include "platform/PlatformExport.h"

namespace blink {

// Mavericks is Mac OS X 10.9, Darwin 13.
PLATFORM_EXPORT bool IsOSMavericks();

// Yosemite is Mac OS X 10.10, Darwin 14.
PLATFORM_EXPORT bool IsOSYosemite();

// El Capitan is Mac OS X 10.11, Darwin 15.
PLATFORM_EXPORT bool IsOSElCapitan();

} // namespace blink

#endif // VersionUtilMac_h
