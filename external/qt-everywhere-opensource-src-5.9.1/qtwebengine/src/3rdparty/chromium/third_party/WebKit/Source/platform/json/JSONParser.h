// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JSONParser_h
#define JSONParser_h

#include "platform/PlatformExport.h"
#include "wtf/text/WTFString.h"

#include <memory>

namespace blink {

class JSONValue;

PLATFORM_EXPORT std::unique_ptr<JSONValue> parseJSON(const String& json);

}  // namespace blink

#endif  // JSONParser_h
