// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MAC_FONT_DESCRIPTOR_H_
#define CONTENT_COMMON_MAC_FONT_DESCRIPTOR_H_

#include "base/strings/string16.h"
#include "content/common/content_export.h"

#ifdef __OBJC__
@class NSFont;
#else
class NSFont;
#endif

// Container to allow serializing an NSFont over IPC.
struct CONTENT_EXPORT FontDescriptor {
  explicit FontDescriptor(NSFont* font);
  FontDescriptor(base::string16 name, float size);

  FontDescriptor() : font_point_size(0) {}

  // Return an autoreleased NSFont corresponding to the font description.
  NSFont* ToNSFont() const;

  // Name of the font.
  base::string16 font_name;

  // Size in points.
  float font_point_size;
};

#endif  // CONTENT_COMMON_MAC_FONT_DESCRIPTOR_H_
