// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/clipboard_message_filter.h"

#import <Cocoa/Cocoa.h>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "content/public/browser/browser_thread.h"
#import "ui/base/cocoa/find_pasteboard.h"

namespace content {
namespace {

// The number of utf16 code units that will be written to the find pasteboard,
// longer texts are silently ignored. This is to prevent that a compromised
// renderer can write unlimited amounts of data into the find pasteboard.
static const size_t kMaxFindPboardStringLength = 4096;

class WriteFindPboardWrapper {
 public:
  explicit WriteFindPboardWrapper(NSString* text)
      : text_([text retain]) {}

  void Run() {
    [[FindPasteboard sharedInstance] setFindText:text_];
  }

 private:
  base::scoped_nsobject<NSString> text_;

  DISALLOW_COPY_AND_ASSIGN(WriteFindPboardWrapper);
};

}  // namespace

// Called on the IO thread.
void ClipboardMessageFilter::OnFindPboardWriteString(
    const base::string16& text) {
  if (text.length() <= kMaxFindPboardStringLength) {
    NSString* nsText = base::SysUTF16ToNSString(text);
    if (nsText) {
      // FindPasteboard must be used on the UI thread.
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE, base::Bind(
              &WriteFindPboardWrapper::Run,
              base::Owned(new WriteFindPboardWrapper(nsText))));
    }
  }
}

}  // namespace content
