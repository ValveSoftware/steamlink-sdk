// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPEN_FROM_CLIPBOARD_CLIPBOARD_RECENT_CONTENT_H_
#define COMPONENTS_OPEN_FROM_CLIPBOARD_CLIPBOARD_RECENT_CONTENT_H_

#include <string>

#include "base/macros.h"
#include "base/time/time.h"

class GURL;

// Helper class returning an URL if the content of the clipboard can be turned
// into an URL, and if it estimates that the content of the clipboard is not too
// old.
class ClipboardRecentContent {
 public:
  ClipboardRecentContent();
  virtual ~ClipboardRecentContent();

  // Returns the global instance of the ClipboardRecentContent singleton. This
  // method does *not* create the singleton and will return null if no instance
  // was registered via SetInstance().
  static ClipboardRecentContent* GetInstance();

  // Sets the global instance of ClipboardRecentContent singleton.
  static void SetInstance(ClipboardRecentContent* instance);

  // Returns true if the clipboard contains a recent URL that has not been
  // supressed, and copies it in |url|. Otherwise, returns false. |url| must not
  // be null.
  virtual bool GetRecentURLFromClipboard(GURL* url) const = 0;

  // Reports that the URL contained in the pasteboard was displayed.
  virtual void RecentURLDisplayed() = 0;

  // Returns how old the content of the clipboard is.
  virtual base::TimeDelta GetClipboardContentAge() const = 0;

  // Prevent GetRecentURLFromClipboard from returning anything until the
  // clipboard's content changed.
  virtual void SuppressClipboardContent() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ClipboardRecentContent);
};

#endif  // COMPONENTS_OPEN_FROM_CLIPBOARD_CLIPBOARD_RECENT_CONTENT_H_
