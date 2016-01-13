// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/font_list.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"

namespace content {

scoped_ptr<base::ListValue> GetFontList_SlowBlocking() {
  base::mac::ScopedNSAutoreleasePool autorelease_pool;
  scoped_ptr<base::ListValue> font_list(new base::ListValue);
  NSFontManager* fontManager = [[[NSFontManager alloc] init] autorelease];
  NSArray* fonts = [fontManager availableFontFamilies];
  for (NSString* family_name in fonts) {
    NSString* localized_family_name =
        [fontManager localizedNameForFamily:family_name face:nil];
    base::ListValue* font_item = new base::ListValue();
    base::string16 family = base::SysNSStringToUTF16(family_name);
    font_item->Append(new base::StringValue(family));
    base::string16 loc_family = base::SysNSStringToUTF16(localized_family_name);
    font_item->Append(new base::StringValue(loc_family));
    font_list->Append(font_item);
  }
  return font_list.Pass();
}

}  // namespace content
