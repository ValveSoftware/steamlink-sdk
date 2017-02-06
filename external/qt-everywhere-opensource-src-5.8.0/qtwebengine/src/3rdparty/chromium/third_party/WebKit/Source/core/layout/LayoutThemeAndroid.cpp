// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutThemeAndroid.h"

namespace blink {

PassRefPtr<LayoutTheme> LayoutThemeAndroid::create()
{
    return adoptRef(new LayoutThemeAndroid());
}

LayoutTheme& LayoutTheme::nativeTheme()
{
    DEFINE_STATIC_REF(LayoutTheme, layoutTheme, (LayoutThemeAndroid::create()));
    return *layoutTheme;
}

LayoutThemeAndroid::~LayoutThemeAndroid()
{
}

} // namespace blink
