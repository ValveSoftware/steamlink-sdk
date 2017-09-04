// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutThemeAndroid_h
#define LayoutThemeAndroid_h

#include "core/layout/LayoutThemeMobile.h"

namespace blink {

class LayoutThemeAndroid final : public LayoutThemeMobile {
 public:
  static PassRefPtr<LayoutTheme> create();
  bool delegatesMenuListRendering() const override { return true; }

 private:
  ~LayoutThemeAndroid() override;
};

}  // namespace blink

#endif  // LayoutThemeAndroid_h
