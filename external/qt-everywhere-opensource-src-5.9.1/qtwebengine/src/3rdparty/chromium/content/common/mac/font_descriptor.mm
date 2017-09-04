// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/mac/font_descriptor.h"

#include <Cocoa/Cocoa.h>

#include "base/strings/sys_string_conversions.h"

FontDescriptor::FontDescriptor(NSFont* font) {
  font_name = base::SysNSStringToUTF16([font fontName]);
  font_point_size = [font pointSize];
}

FontDescriptor::FontDescriptor(base::string16 name, float size) {
  font_name = name;
  font_point_size = size;
}

NSFont* FontDescriptor::ToNSFont() const {
  NSString* font_name_ns = base::SysUTF16ToNSString(font_name);
  NSFont* font = [NSFont fontWithName:font_name_ns size:font_point_size];
  return font;
}
