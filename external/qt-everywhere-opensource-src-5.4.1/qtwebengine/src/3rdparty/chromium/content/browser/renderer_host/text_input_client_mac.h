// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_TEXT_INPUT_CLIENT_MAC_H_
#define CONTENT_BROWSER_RENDERER_HOST_TEXT_INPUT_CLIENT_MAC_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_block.h"
#include "base/mac/scoped_nsobject.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "ui/gfx/point.h"

template <typename T> struct DefaultSingletonTraits;

namespace content {
class RenderWidgetHost;

// This class helps with the Mac OS X dictionary popup. For the design overview,
// look at this document:
//   http://dev.chromium.org/developers/design-documents/system-dictionary-pop-up-architecture
//
// This service is used to marshall information for these three methods that are
// implemented in RenderWidgetHostViewMac:
//   -[NSTextInput characterIndexForPoint:]
//   -[NSTextInput attributedSubstringFromRange:]
//   -[NSTextInput firstRectForCharacterRange:]
//
// Because these methods are part of a synchronous system API, implementing them
// requires getting information from the renderer synchronously. Rather than
// using an actual sync IPC message, a normal async ViewMsg is used with a lock
// and condition (managed by this service).
//
// Mac OS 10.8 introduced -[NSResponder quickLookWithEvent:].
// We can use it to implement asynchronous dictionary lookup when the user
// taps a word using three fingers.
// But currently the "Look Up in Dictionary" context menu item still goes
// through the above synchronous IPC.
class CONTENT_EXPORT TextInputClientMac {
 public:
  // Returns the singleton instance.
  static TextInputClientMac* GetInstance();

  // Each of the three methods mentioned above has an associated pair of methods
  // to get data from the renderer. The Get*() methods block the calling thread
  // (always the UI thread) with a short timeout after the async message has
  // been sent to the renderer to lookup the information needed to respond to
  // the system. The Set*AndSignal() methods store the looked up information in
  // this service and signal the condition to allow the Get*() methods to
  // unlock and return that stored value.
  //
  // Returns NSNotFound if the request times out or is not completed.
  NSUInteger GetCharacterIndexAtPoint(RenderWidgetHost* rwh, gfx::Point point);
  // Returns nil if the request times out or is completed.
  NSAttributedString* GetAttributedSubstringFromRange(
      RenderWidgetHost* rwh, NSRange range);
  // Returns NSZeroRect if the request times out or is not completed. The result
  // is in WebKit coordinates.
  NSRect GetFirstRectForRange(RenderWidgetHost* rwh, NSRange range);

  // When the renderer sends the ViewHostMsg reply, the RenderMessageFilter will
  // call the corresponding method on the IO thread to unlock the condition and
  // allow the Get*() methods to continue/return.
  void SetCharacterIndexAndSignal(NSUInteger index);
  void SetFirstRectAndSignal(NSRect first_rect);
  void SetSubstringAndSignal(NSAttributedString* string);

  // This async method is invoked from RenderWidgetHostViewCocoa's
  // -quickLookWithEvent:, when the user taps a word using 3 fingers.
  // The reply callback will be invoked from the IO thread, the caller is
  // responsible for bouncing to the main thread if necessary.
  // The callback parameters provide the attributed word under the point and
  // the lower left baseline point of the text.
  void GetStringAtPoint(RenderWidgetHost* rwh,
                        gfx::Point point,
                        void (^replyHandler)(NSAttributedString*, NSPoint));
  // This is called on the IO thread when we get the renderer's reply for
  // GetStringAtPoint.
  void GetStringAtPointReply(NSAttributedString*, NSPoint);

 private:
  friend struct DefaultSingletonTraits<TextInputClientMac>;
  TextInputClientMac();
  ~TextInputClientMac();

  // The critical sections that the Condition guards are in Get*() methods.
  // These methods lock the internal condition for use before the asynchronous
  // message is sent to the renderer to lookup the required information. These
  // are only used on the UI thread.
  void BeforeRequest();
  // Called at the end of a critical section. This will release the lock and
  // condition.
  void AfterRequest();

  NSUInteger character_index_;
  NSRect first_rect_;
  base::scoped_nsobject<NSAttributedString> substring_;

  base::Lock lock_;
  base::ConditionVariable condition_;

  base::mac::ScopedBlock<void(^)(NSAttributedString*, NSPoint)> replyHandler_;

  DISALLOW_COPY_AND_ASSIGN(TextInputClientMac);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_TEXT_INPUT_CLIENT_MAC_H_
