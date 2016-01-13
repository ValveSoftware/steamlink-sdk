// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/web_contents/web_drag_source_mac.h"

#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

typedef RenderViewHostTestHarness WebDragSourceMacTest;

TEST_F(WebDragSourceMacTest, DragInvalidlyEscapedBookmarklet) {
  scoped_ptr<WebContents> contents(CreateTestWebContents());
  base::scoped_nsobject<NSView> view(
      [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 10, 10)]);

  scoped_ptr<DropData> dropData(new DropData);
  dropData->url = GURL("javascript:%");

  WebContentsImpl* contentsImpl = static_cast<WebContentsImpl*>(contents.get());
  base::scoped_nsobject<WebDragSource> source([[WebDragSource alloc]
      initWithContents:contentsImpl
                  view:view
              dropData:dropData.get()
                 image:nil
                offset:NSZeroPoint
            pasteboard:[NSPasteboard pasteboardWithUniqueName]
     dragOperationMask:NSDragOperationCopy]);

  // Test that this call doesn't throw any exceptions: http://crbug.com/128371
  base::scoped_nsobject<NSPasteboard> pasteboard(
      [NSPasteboard pasteboardWithUniqueName]);
  [source lazyWriteToPasteboard:pasteboard forType:NSURLPboardType];
}

}  // namespace content
