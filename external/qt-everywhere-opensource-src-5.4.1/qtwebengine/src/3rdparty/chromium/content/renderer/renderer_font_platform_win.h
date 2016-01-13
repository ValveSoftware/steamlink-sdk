// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_RENDERER_FONT_PLATFORM_WIN_H_
#define CHROME_RENDERER_RENDERER_FONT_PLATFORM_WIN_H_

struct IDWriteFactory;
struct IDWriteFontCollection;

namespace content {

IDWriteFontCollection* GetCustomFontCollection(IDWriteFactory* factory);

}  // namespace content

#endif  // CHROME_RENDERER_RENDERER_FONT_PLATFORM_WIN_H_
