// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "content/renderer/pepper/pepper_truetype_font.h"

namespace content {

// static
PepperTrueTypeFont* PepperTrueTypeFont::Create(
    const ppapi::proxy::SerializedTrueTypeFontDesc& desc) {
  NOTIMPLEMENTED();  // Font API isn't implemented on Android.
  return 0;
}

}  // namespace content
