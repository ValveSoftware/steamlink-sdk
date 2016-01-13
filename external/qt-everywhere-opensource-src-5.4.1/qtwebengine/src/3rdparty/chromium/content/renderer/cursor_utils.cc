// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/cursor_utils.h"

#include "build/build_config.h"
#include "content/common/cursors/webcursor.h"
#include "third_party/WebKit/public/platform/WebCursorInfo.h"

using blink::WebCursorInfo;

namespace content {

bool GetWebKitCursorInfo(const WebCursor& cursor,
                         WebCursorInfo* webkit_cursor_info) {
  WebCursor::CursorInfo cursor_info;
  cursor.GetCursorInfo(&cursor_info);

  webkit_cursor_info->type = cursor_info.type;
  webkit_cursor_info->hotSpot = cursor_info.hotspot;
  webkit_cursor_info->customImage = cursor_info.custom_image;
  webkit_cursor_info->imageScaleFactor = cursor_info.image_scale_factor;
#if defined(OS_WIN)
  webkit_cursor_info->externalHandle = cursor_info.external_handle;
#endif
  return true;
}

void InitializeCursorFromWebKitCursorInfo(
    WebCursor* cursor,
    const WebCursorInfo& webkit_cursor_info) {
  WebCursor::CursorInfo web_cursor_info;
  web_cursor_info.type = webkit_cursor_info.type;
  web_cursor_info.image_scale_factor = webkit_cursor_info.imageScaleFactor;
  web_cursor_info.hotspot = webkit_cursor_info.hotSpot;
  web_cursor_info.custom_image = webkit_cursor_info.customImage.getSkBitmap();
#if defined(OS_WIN)
  web_cursor_info.external_handle = webkit_cursor_info.externalHandle;
#endif
  cursor->InitFromCursorInfo(web_cursor_info);
}

}  // namespce content
