// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_FONT_SMOOTHING_WIN_H_
#define UI_GFX_FONT_SMOOTHING_WIN_H_

namespace gfx {

// Returns the Windows system font smoothing and ClearType settings.
void GetCachedFontSmoothingSettings(bool* smoothing_enabled,
                                    bool* cleartype_enabled);

}  // namespace gfx

#endif  // UI_GFX_FONT_SMOOTHING_WIN_H_
