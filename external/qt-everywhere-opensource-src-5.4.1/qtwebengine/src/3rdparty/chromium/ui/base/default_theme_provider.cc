// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/default_theme_provider.h"

#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

namespace ui {

DefaultThemeProvider::DefaultThemeProvider() {}

DefaultThemeProvider::~DefaultThemeProvider() {}

bool DefaultThemeProvider::UsingSystemTheme() const {
  return true;
}

gfx::ImageSkia* DefaultThemeProvider::GetImageSkiaNamed(int id) const {
  return ResourceBundle::GetSharedInstance().GetImageSkiaNamed(id);
}

SkColor DefaultThemeProvider::GetColor(int id) const {
  // Return debugging-blue.
  return 0xff0000ff;
}

int DefaultThemeProvider::GetDisplayProperty(int id) const {
  return -1;
}

bool DefaultThemeProvider::ShouldUseNativeFrame() const {
  return false;
}

bool DefaultThemeProvider::HasCustomImage(int id) const {
  return false;
}

base::RefCountedMemory* DefaultThemeProvider::GetRawData(
    int id,
    ui::ScaleFactor scale_factor) const {
  return NULL;
}

}  // namespace ui
