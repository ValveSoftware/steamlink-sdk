// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NetworkUtils_h
#define NetworkUtils_h

#include "platform/PlatformExport.h"
#include "wtf/Forward.h"

namespace blink {

namespace NetworkUtils {

PLATFORM_EXPORT bool isReservedIPAddress(const String& host);

} // NetworkUtils

} // namespace blink

#endif // NetworkUtils_h
