// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/open_from_clipboard/clipboard_recent_content_ios.h"

#import <CommonCrypto/CommonDigest.h>
#include <stddef.h>
#include <stdint.h>
#import <UIKit/UIKit.h>

#import "base/ios/ios_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/sys_string_conversions.h"
#include "base/sys_info.h"
#include "url/gurl.h"
#include "url/url_constants.h"

// Bridge that forwards UIApplicationDidBecomeActiveNotification notifications
// to its delegate.
@interface ApplicationDidBecomeActiveNotificationListenerBridge : NSObject

// Initialize the ApplicationDidBecomeActiveNotificationListenerBridge with
// |delegate| which must not be null.
- (instancetype)initWithDelegate:(ClipboardRecentContentIOS*)delegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

@implementation ApplicationDidBecomeActiveNotificationListenerBridge {
  ClipboardRecentContentIOS* _delegate;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithDelegate:(ClipboardRecentContentIOS*)delegate {
  DCHECK(delegate);
  self = [super init];
  if (self) {
    _delegate = delegate;
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(didBecomeActive:)
               name:UIApplicationDidBecomeActiveNotification
             object:nil];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)didBecomeActive:(NSNotification*)notification {
  if (_delegate) {
    _delegate->LoadFromUserDefaults();
    _delegate->UpdateIfNeeded();
  }
}

- (void)disconnect {
  _delegate = nullptr;
}

@end

namespace {
// Key used to store the pasteboard's current change count. If when resuming
// chrome the pasteboard's change count is different from the stored one, then
// it means that the pasteboard's content has changed.
NSString* kPasteboardChangeCountKey = @"PasteboardChangeCount";
// Key used to store the last date at which it was detected that the pasteboard
// changed. It is used to evaluate the age of the pasteboard's content.
NSString* kPasteboardChangeDateKey = @"PasteboardChangeDate";
// Key used to store the hash of the content of the pasteboard. Whenever the
// hash changed, the pasteboard content is considered to have changed.
NSString* kPasteboardEntryMD5Key = @"PasteboardEntryMD5";
base::TimeDelta kMaximumAgeOfClipboard = base::TimeDelta::FromHours(3);
// Schemes accepted by the ClipboardRecentContentIOS.
const char* kAuthorizedSchemes[] = {
    url::kHttpScheme,
    url::kHttpsScheme,
    url::kDataScheme,
    url::kAboutScheme,
};

// Compute a hash consisting of the first 4 bytes of the MD5 hash of |string|.
// This value is used to detect pasteboard content change. Keeping only 4 bytes
// is a privacy requirement to introduce collision and allow deniability of
// having copied a given string.
NSData* WeakMD5FromNSString(NSString* string) {
  unsigned char hash[CC_MD5_DIGEST_LENGTH];
  const std::string clipboard = base::SysNSStringToUTF8(string);
  const char* c_string = clipboard.c_str();
  CC_MD5(c_string, strlen(c_string), hash);
  NSData* data = [NSData dataWithBytes:hash length:4];
  return data;
}

}  // namespace

bool ClipboardRecentContentIOS::GetRecentURLFromClipboard(GURL* url) {
  DCHECK(url);
  UpdateIfNeeded();
  if (GetClipboardContentAge() > kMaximumAgeOfClipboard) {
    return false;
  }

  GURL url_from_pasteboard = URLFromPasteboard();
  if (url_from_pasteboard.is_valid()) {
    *url = url_from_pasteboard;
    return true;
  }
  return false;
}

base::TimeDelta ClipboardRecentContentIOS::GetClipboardContentAge() const {
  return base::TimeDelta::FromSeconds(static_cast<int64_t>(
      -[last_pasteboard_change_date_ timeIntervalSinceNow]));
}

void ClipboardRecentContentIOS::SuppressClipboardContent() {
  // User cleared the user data. The pasteboard entry must be removed from the
  // omnibox list. Force entry expiration by setting copy date to 1970.
  last_pasteboard_change_date_.reset(
      [[NSDate alloc] initWithTimeIntervalSince1970:0]);
  SaveToUserDefaults();
}

void ClipboardRecentContentIOS::UpdateIfNeeded() {
  if (!HasPasteboardChanged())
    return;

  base::RecordAction(base::UserMetricsAction("MobileOmniboxClipboardChanged"));

  GURL url_from_pasteboard = URLFromPasteboard();
  last_pasteboard_change_date_.reset([[NSDate date] retain]);
  last_pasteboard_change_count_ = [UIPasteboard generalPasteboard].changeCount;
  NSString* pasteboard_string = [[UIPasteboard generalPasteboard] string];
  if (!pasteboard_string) {
    pasteboard_string = @"";
  }
  NSData* MD5 = WeakMD5FromNSString(pasteboard_string);
  last_pasteboard_entry_md5_.reset([MD5 retain]);
  SaveToUserDefaults();
}

ClipboardRecentContentIOS::ClipboardRecentContentIOS(
    const std::string& application_scheme,
    NSUserDefaults* group_user_defaults)
    : application_scheme_(application_scheme),
      shared_user_defaults_([group_user_defaults retain]) {
  last_pasteboard_change_count_ = NSIntegerMax;
  LoadFromUserDefaults();

  UpdateIfNeeded();

  // Makes sure |last_pasteboard_change_count_| was properly initialized.
  DCHECK_NE(last_pasteboard_change_count_, NSIntegerMax);
  notification_bridge_.reset(
      [[ApplicationDidBecomeActiveNotificationListenerBridge alloc]
          initWithDelegate:this]);
}

bool ClipboardRecentContentIOS::HasPasteboardChanged() const {
  // If |MD5Changed|, we know for sure there has been at least one pasteboard
  // copy since last time it was checked.
  // If the pasteboard content is still the same but the device was not
  // rebooted, the change count can be checked to see if it changed.
  // Note: due to a mismatch between the actual behavior and documentation, and
  // lack of consistency on different reboot scenarios, the change count cannot
  // be checked after a reboot.
  // See radar://21833556 for more information.
  NSInteger change_count = [UIPasteboard generalPasteboard].changeCount;
  bool change_count_changed = change_count != last_pasteboard_change_count_;

  bool not_rebooted = Uptime() > GetClipboardContentAge();
  if (not_rebooted)
    return change_count_changed;

  NSString* pasteboard_string = [[UIPasteboard generalPasteboard] string];
  if (!pasteboard_string) {
    pasteboard_string = @"";
  }
  NSData* md5 = WeakMD5FromNSString(pasteboard_string);
  BOOL md5_changed = ![md5 isEqualToData:last_pasteboard_entry_md5_];

  return md5_changed;
}

ClipboardRecentContentIOS::~ClipboardRecentContentIOS() {
  [notification_bridge_ disconnect];
}

GURL ClipboardRecentContentIOS::URLFromPasteboard() {
  NSString* clipboard_string = [[UIPasteboard generalPasteboard] string];
  if (!clipboard_string) {
    return GURL::EmptyGURL();
  }
  const std::string clipboard = base::SysNSStringToUTF8(clipboard_string);
  GURL gurl = GURL(clipboard);
  if (gurl.is_valid()) {
    for (size_t i = 0; i < arraysize(kAuthorizedSchemes); ++i) {
      if (gurl.SchemeIs(kAuthorizedSchemes[i])) {
        return gurl;
      }
    }
    if (!application_scheme_.empty() &&
        gurl.SchemeIs(application_scheme_.c_str())) {
      return gurl;
    }
  }
  return GURL::EmptyGURL();
}

void ClipboardRecentContentIOS::LoadFromUserDefaults() {
  last_pasteboard_change_count_ =
      [shared_user_defaults_ integerForKey:kPasteboardChangeCountKey];
  last_pasteboard_change_date_.reset(
      [[shared_user_defaults_ objectForKey:kPasteboardChangeDateKey] retain]);
  last_pasteboard_entry_md5_.reset(
      [[shared_user_defaults_ objectForKey:kPasteboardEntryMD5Key] retain]);

  DCHECK(!last_pasteboard_change_date_ ||
         [last_pasteboard_change_date_ isKindOfClass:[NSDate class]]);
}

void ClipboardRecentContentIOS::SaveToUserDefaults() {
  [shared_user_defaults_ setInteger:last_pasteboard_change_count_
                             forKey:kPasteboardChangeCountKey];
  [shared_user_defaults_ setObject:last_pasteboard_change_date_
                            forKey:kPasteboardChangeDateKey];
  [shared_user_defaults_ setObject:last_pasteboard_entry_md5_
                            forKey:kPasteboardEntryMD5Key];
}

base::TimeDelta ClipboardRecentContentIOS::Uptime() const {
  return base::SysInfo::Uptime();
}
