// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/sys_string_conversions.h"
#import "content/browser/renderer_host/render_widget_host_view_mac_dictionary_helper.h"
#import "content/browser/renderer_host/render_widget_host_view_mac.h"

namespace content {

RenderWidgetHostViewMacDictionaryHelper::
    RenderWidgetHostViewMacDictionaryHelper(RenderWidgetHostView* view)
    : view_(static_cast<RenderWidgetHostViewMac*>(view)),
      target_view_(static_cast<RenderWidgetHostViewMac*>(view)) {
}

void RenderWidgetHostViewMacDictionaryHelper::SetTargetView(
    RenderWidgetHostView* target_view) {
  target_view_ = static_cast<RenderWidgetHostViewMac*>(target_view);
}

void RenderWidgetHostViewMacDictionaryHelper::ShowDefinitionForSelection() {
  NSRange selection_range = [view_->cocoa_view() selectedRange];
  NSAttributedString* attr_string =
      [view_->cocoa_view() attributedSubstringForProposedRange:selection_range
                                           actualRange:nil];
  if (!attr_string) {
    if (view_->selected_text().empty())
      return;
    // The PDF plugin does not support getting the attributed string. Until it
    // does, use NSPerformService(), which opens Dictionary.app.
    // http://crbug.com/152438
    // TODO(asvitkine): This should be removed after the above support is added.
    NSString* text = base::SysUTF8ToNSString(view_->selected_text());
    NSPasteboard* pasteboard = [NSPasteboard pasteboardWithUniqueName];
    NSArray* types = [NSArray arrayWithObject:NSStringPboardType];
    [pasteboard declareTypes:types owner:nil];
    if ([pasteboard setString:text forType:NSStringPboardType])
      NSPerformService(@"Look Up in Dictionary", pasteboard);
    return;
  }

  NSRect rect =
      [view_->cocoa_view() firstViewRectForCharacterRange:selection_range
                                              actualRange:nil];

  NSDictionary* attrs = [attr_string attributesAtIndex:0 effectiveRange:nil];
  NSFont* font = [attrs objectForKey:NSFontAttributeName];
  rect.origin.y += NSHeight(rect) - [font ascender];

  rect.origin.x += offset_.x();
  rect.origin.y += offset_.y();

  [target_view_->cocoa_view() showDefinitionForAttributedString:attr_string
                                                        atPoint:rect.origin];
}

}  // namespace content
