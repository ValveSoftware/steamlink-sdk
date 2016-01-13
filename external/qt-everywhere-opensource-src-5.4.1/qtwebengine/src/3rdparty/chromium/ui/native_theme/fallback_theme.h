// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_FALLBACK_THEME_H_
#define UI_NATIVE_THEME_FALLBACK_THEME_H_

#include "ui/native_theme/native_theme_base.h"

namespace ui {

// This theme can draw UI controls on every platform. This is only used when
// zooming a web page and the native theme doesn't support scaling.
class NATIVE_THEME_EXPORT FallbackTheme : public NativeThemeBase {
 public:
  FallbackTheme();
  virtual ~FallbackTheme();

 protected:
  // Overridden from NativeThemeBase:
  virtual SkColor GetSystemColor(ColorId color_id) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(FallbackTheme);
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_FALLBACK_THEME_H_
