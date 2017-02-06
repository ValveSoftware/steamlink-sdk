// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FilePathConversion_h
#define FilePathConversion_h

#include "WebCommon.h"

namespace base {
class FilePath;
}

namespace blink {

class WebString;

BLINK_COMMON_EXPORT base::FilePath WebStringToFilePath(const WebString&);

} // namespace blink

#endif // FilePathConversion_h
