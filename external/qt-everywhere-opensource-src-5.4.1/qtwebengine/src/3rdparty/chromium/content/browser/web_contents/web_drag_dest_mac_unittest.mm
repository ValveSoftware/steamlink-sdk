// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/scoped_nsautorelease_pool.h"
#import "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "content/browser/web_contents/web_drag_dest_mac.h"
#include "content/public/common/drop_data.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "third_party/mozilla/NSPasteboard+Utils.h"
#import "ui/base/dragdrop/cocoa_dnd_util.h"
#import "ui/gfx/test/ui_cocoa_test_helper.h"

using content::DropData;
using content::RenderViewHostImplTestHarness;

namespace {
NSString* const kCrCorePasteboardFlavorType_url =
    @"CorePasteboardFlavorType 0x75726C20"; // 'url '  url
NSString* const kCrCorePasteboardFlavorType_urln =
    @"CorePasteboardFlavorType 0x75726C6E"; // 'urln'  title
}  // namespace

class WebDragDestTest : public RenderViewHostImplTestHarness {
 public:
  virtual void SetUp() {
    RenderViewHostImplTestHarness::SetUp();
    drag_dest_.reset([[WebDragDest alloc] initWithWebContentsImpl:contents()]);
  }

  void PutURLOnPasteboard(NSString* urlString, NSPasteboard* pboard) {
    [pboard declareTypes:[NSArray arrayWithObject:NSURLPboardType]
                   owner:nil];
    NSURL* url = [NSURL URLWithString:urlString];
    EXPECT_TRUE(url);
    [url writeToPasteboard:pboard];
  }

  void PutCoreURLAndTitleOnPasteboard(NSString* urlString, NSString* title,
                                      NSPasteboard* pboard) {
    [pboard
        declareTypes:[NSArray arrayWithObjects:kCrCorePasteboardFlavorType_url,
                                               kCrCorePasteboardFlavorType_urln,
                                               nil]
               owner:nil];
    [pboard setString:urlString
              forType:kCrCorePasteboardFlavorType_url];
    [pboard setString:title
              forType:kCrCorePasteboardFlavorType_urln];
  }

  base::mac::ScopedNSAutoreleasePool pool_;
  base::scoped_nsobject<WebDragDest> drag_dest_;
};

// Make sure nothing leaks.
TEST_F(WebDragDestTest, Init) {
  EXPECT_TRUE(drag_dest_);
}

// Test flipping of coordinates given a point in window coordinates.
TEST_F(WebDragDestTest, Flip) {
  NSPoint windowPoint = NSZeroPoint;
  base::scoped_nsobject<NSWindow> window([[CocoaTestHelperWindow alloc] init]);
  NSPoint viewPoint =
      [drag_dest_ flipWindowPointToView:windowPoint
                                   view:[window contentView]];
  NSPoint screenPoint =
      [drag_dest_ flipWindowPointToScreen:windowPoint
                                     view:[window contentView]];
  EXPECT_EQ(0, viewPoint.x);
  EXPECT_EQ(600, viewPoint.y);
  EXPECT_EQ(0, screenPoint.x);
  // We can't put a value on the screen size since everyone will have a
  // different one.
  EXPECT_NE(0, screenPoint.y);
}

TEST_F(WebDragDestTest, URL) {
  NSPasteboard* pboard = nil;
  NSString* url = nil;
  NSString* title = nil;
  GURL result_url;
  base::string16 result_title;

  // Put a URL on the pasteboard and check it.
  pboard = [NSPasteboard pasteboardWithUniqueName];
  url = @"http://www.google.com/";
  PutURLOnPasteboard(url, pboard);
  EXPECT_TRUE(ui::PopulateURLAndTitleFromPasteboard(
      &result_url, &result_title, pboard, NO));
  EXPECT_EQ(base::SysNSStringToUTF8(url), result_url.spec());
  [pboard releaseGlobally];

  // Put a 'url ' and 'urln' on the pasteboard and check it.
  pboard = [NSPasteboard pasteboardWithUniqueName];
  url = @"http://www.google.com/";
  title = @"Title of Awesomeness!",
  PutCoreURLAndTitleOnPasteboard(url, title, pboard);
  EXPECT_TRUE(ui::PopulateURLAndTitleFromPasteboard(
      &result_url, &result_title, pboard, NO));
  EXPECT_EQ(base::SysNSStringToUTF8(url), result_url.spec());
  EXPECT_EQ(base::SysNSStringToUTF16(title), result_title);
  [pboard releaseGlobally];

  // Also check that it passes file:// via 'url '/'urln' properly.
  pboard = [NSPasteboard pasteboardWithUniqueName];
  url = @"file:///tmp/dont_delete_me.txt";
  title = @"very important";
  PutCoreURLAndTitleOnPasteboard(url, title, pboard);
  EXPECT_TRUE(ui::PopulateURLAndTitleFromPasteboard(
      &result_url, &result_title, pboard, NO));
  EXPECT_EQ(base::SysNSStringToUTF8(url), result_url.spec());
  EXPECT_EQ(base::SysNSStringToUTF16(title), result_title);
  [pboard releaseGlobally];

  // And javascript:.
  pboard = [NSPasteboard pasteboardWithUniqueName];
  url = @"javascript:open('http://www.youtube.com/')";
  title = @"kill some time";
  PutCoreURLAndTitleOnPasteboard(url, title, pboard);
  EXPECT_TRUE(ui::PopulateURLAndTitleFromPasteboard(
      &result_url, &result_title, pboard, NO));
  EXPECT_EQ(base::SysNSStringToUTF8(url), result_url.spec());
  EXPECT_EQ(base::SysNSStringToUTF16(title), result_title);
  [pboard releaseGlobally];

  pboard = [NSPasteboard pasteboardWithUniqueName];
  url = @"/bin/sh";
  [pboard declareTypes:[NSArray arrayWithObject:NSFilenamesPboardType]
                 owner:nil];
  [pboard setPropertyList:[NSArray arrayWithObject:url]
                  forType:NSFilenamesPboardType];
  EXPECT_FALSE(ui::PopulateURLAndTitleFromPasteboard(
      &result_url, &result_title, pboard, NO));
  EXPECT_TRUE(ui::PopulateURLAndTitleFromPasteboard(
      &result_url, &result_title, pboard, YES));
  EXPECT_EQ("file://localhost/bin/sh", result_url.spec());
  EXPECT_EQ("sh", base::UTF16ToUTF8(result_title));
  [pboard releaseGlobally];
}

TEST_F(WebDragDestTest, Data) {
  DropData data;
  NSPasteboard* pboard = [NSPasteboard pasteboardWithUniqueName];

  PutURLOnPasteboard(@"http://www.google.com", pboard);
  [pboard addTypes:[NSArray arrayWithObjects:NSHTMLPboardType,
                              NSStringPboardType, nil]
             owner:nil];
  NSString* htmlString = @"<html><body><b>hi there</b></body></html>";
  NSString* textString = @"hi there";
  [pboard setString:htmlString forType:NSHTMLPboardType];
  [pboard setString:textString forType:NSStringPboardType];
  [drag_dest_ populateDropData:&data fromPasteboard:pboard];
  EXPECT_EQ(data.url.spec(), "http://www.google.com/");
  EXPECT_EQ(base::SysNSStringToUTF16(textString), data.text.string());
  EXPECT_EQ(base::SysNSStringToUTF16(htmlString), data.html.string());

  [pboard releaseGlobally];
}
