// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_RENDER_FONT_WARMUP_WIN_H_
#define CONTENT_PUBLIC_RENDERER_RENDER_FONT_WARMUP_WIN_H_

#include "content/common/content_export.h"

class SkFontMgr;
class SkTypeface;

namespace content {

// Make necessary calls to cache the data for a given font, used before
// sandbox lockdown.
CONTENT_EXPORT void DoPreSandboxWarmupForTypeface(SkTypeface* typeface);

// Get the shared font manager used during pre-sandbox warmup for DirectWrite
// fonts.
CONTENT_EXPORT SkFontMgr* GetPreSandboxWarmupFontMgr();

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_RENDER_FONT_WARMUP_WIN_H_
