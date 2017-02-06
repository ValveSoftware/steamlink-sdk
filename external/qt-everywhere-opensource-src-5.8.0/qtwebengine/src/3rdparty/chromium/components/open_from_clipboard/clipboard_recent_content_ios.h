// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPEN_FROM_CLIPBOARD_CLIPBOARD_RECENT_CONTENT_IOS_H_
#define COMPONENTS_OPEN_FROM_CLIPBOARD_CLIPBOARD_RECENT_CONTENT_IOS_H_

#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "components/open_from_clipboard/clipboard_recent_content.h"
#include "url/gurl.h"

@class NSDate;
@class NSUserDefaults;
@class PasteboardNotificationListenerBridge;

class ClipboardRecentContentIOSTest;

// IOS implementation of ClipboardRecentContent
class ClipboardRecentContentIOS : public ClipboardRecentContent {
 public:
  // |application_scheme| is the URL scheme that can be used to open the
  // current application, may be empty if no such scheme exists. Used to
  // determine whether or not the clipboard contains a relevant URL.
  // |group_user_defaults| is the NSUserDefaults used to store information on
  // pasteboard entry expiration. This information will be shared with other
  // application in the application group.
  explicit ClipboardRecentContentIOS(const std::string& application_scheme,
                                     NSUserDefaults* group_user_defaults);
  ~ClipboardRecentContentIOS() override;

  // Notifies that the content of the pasteboard may have changed.
  void PasteboardChanged();

  // Checks if pasteboard changed since last time a pasteboard change was
  // registered.
  bool HasPasteboardChanged(base::TimeDelta uptime);

  // Gets the current URL in the clipboard. If the cache is out of date, updates
  // it.
  bool GetCurrentURLFromClipboard(GURL* url);

  // Loads information from the user defaults about the latest pasteboard entry.
  void LoadFromUserDefaults();

  // ClipboardRecentContent implementation.
  bool GetRecentURLFromClipboard(GURL* url) const override;

  base::TimeDelta GetClipboardContentAge() const override;
  void SuppressClipboardContent() override;
  void RecentURLDisplayed() override;

 private:
  friend class ClipboardRecentContentIOSTest;

  // Helper constructor for testing. |uptime| is how long ago the device has
  // started, while |application_scheme| has the same meaning as the public
  // constructor.
  ClipboardRecentContentIOS(const std::string& application_scheme,
                            base::TimeDelta uptime);

  // Initializes the object. |uptime| is how long ago the device has started.
  void Init(base::TimeDelta uptime);

  // Saves information to the user defaults about the latest pasteboard entry.
  void SaveToUserDefaults();

  // Returns the URL contained in the clipboard (if any).
  GURL URLFromPasteboard();

  // Contains the URL scheme opening the app. May be empty.
  std::string application_scheme_;
  // The pasteboard's change count. Increases everytime the pasteboard changes.
  NSInteger last_pasteboard_change_count_;
  // Estimation of the date when the pasteboard changed.
  base::scoped_nsobject<NSDate> last_pasteboard_change_date_;
  // Estimation of the copy date of the last displayed URL.
  base::scoped_nsobject<NSDate> last_displayed_pasteboard_entry_;
  // MD5 hash of the last registered pasteboard entry.
  base::scoped_nsobject<NSData> last_pasteboard_entry_md5_;
  // Cache of the GURL contained in the pasteboard (if any).
  GURL url_from_pasteboard_cache_;
  // Bridge to receive notification when the pasteboard changes.
  base::scoped_nsobject<PasteboardNotificationListenerBridge>
      notification_bridge_;
  // The user defaults from the app group used to optimize the pasteboard change
  // detection.
  base::scoped_nsobject<NSUserDefaults> shared_user_defaults_;

  DISALLOW_COPY_AND_ASSIGN(ClipboardRecentContentIOS);
};

#endif  // COMPONENTS_OPEN_FROM_CLIPBOARD_CLIPBOARD_RECENT_CONTENT_IOS_H_
