// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/network/ResourceError.h"
#include <ostream>

// Pretty printer for gtest.
// Each corresponding declaration should be placed in the header file of
// the corresponding class (e.g. ResourceError.h) to avoid ODR violations,
// not in e.g. PlatformTestPrinters.h. See https://crbug.com/514330.

namespace blink {

std::ostream& operator<<(std::ostream& os, const ResourceError& error)
{
    return os
        << "domain = " << error.domain()
        << ", errorCode = " << error.errorCode()
        << ", failingURL = " << error.failingURL()
        << ", localizedDescription = " << error.localizedDescription()
        << ", isNull = " << error.isNull()
        << ", isCancellation = " << error.isCancellation()
        << ", isAccessCheck = " << error.isAccessCheck()
        << ", isTimeout = " << error.isTimeout()
        << ", staleCopyInCache = " << error.staleCopyInCache()
        << ", wasIgnoredByHandler = " << error.wasIgnoredByHandler();
}

} // namespace blink
