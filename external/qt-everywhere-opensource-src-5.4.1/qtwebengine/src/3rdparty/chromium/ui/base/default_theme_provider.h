// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_DEFAULT_THEME_PROVIDER_H_
#define UI_BASE_DEFAULT_THEME_PROVIDER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/base/theme_provider.h"
#include "ui/base/ui_base_export.h"

namespace ui {
class ResourceBundle;
}

namespace ui {

class UI_BASE_EXPORT DefaultThemeProvider : public ThemeProvider {
 public:
  DefaultThemeProvider();
  virtual ~DefaultThemeProvider();

  // Overridden from ui::ThemeProvider:
  virtual bool UsingSystemTheme() const OVERRIDE;
  virtual gfx::ImageSkia* GetImageSkiaNamed(int id) const OVERRIDE;
  virtual SkColor GetColor(int id) const OVERRIDE;
  virtual int GetDisplayProperty(int id) const OVERRIDE;
  virtual bool ShouldUseNativeFrame() const OVERRIDE;
  virtual bool HasCustomImage(int id) const OVERRIDE;
  virtual base::RefCountedMemory* GetRawData(
      int id,
      ui::ScaleFactor scale_factor) const OVERRIDE;

#if defined(OS_MACOSX) && !defined(TOOLKIT_VIEWS)
  virtual NSImage* GetNSImageNamed(int id) const OVERRIDE;
  virtual NSColor* GetNSImageColorNamed(int id) const OVERRIDE;
  virtual NSColor* GetNSColor(int id) const OVERRIDE;
  virtual NSColor* GetNSColorTint(int id) const OVERRIDE;
  virtual NSGradient* GetNSGradient(int id) const OVERRIDE;
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultThemeProvider);
};

}  // namespace ui

#endif  // UI_BASE_DEFAULT_THEME_PROVIDER_H_
