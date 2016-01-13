// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CHILD_PROCESS_SANDBOX_SUPPORT_IMPL_LINUX_H_
#define CONTENT_COMMON_CHILD_PROCESS_SANDBOX_SUPPORT_IMPL_LINUX_H_

#include "base/posix/global_descriptors.h"
#include "content/public/common/child_process_sandbox_support_linux.h"
#include "content/public/common/content_descriptors.h"

namespace blink {
struct WebFallbackFont;
struct WebFontRenderStyle;
}

namespace content {

// Return a font family which provides glyphs for the Unicode code point
// specified by |character|
//   character: a UTF32 character
//   preferred_locale: preferred locale identifier for the |character|
//
// Returns: a font family instance.
// The instance has an empty font name if the request could not be satisfied.
void GetFallbackFontForCharacter(const int32_t character,
                               const char* preferred_locale,
                               blink::WebFallbackFont* family);

void GetRenderStyleForStrike(const char* family, int sizeAndStyle,
                             blink::WebFontRenderStyle* out);

inline int GetSandboxFD() {
  return kSandboxIPCChannel + base::GlobalDescriptors::kBaseDescriptor;
}

};  // namespace content

#endif  // CONTENT_COMMON_CHILD_PROCESS_SANDBOX_SUPPORT_IMPL_LINUX_H_
