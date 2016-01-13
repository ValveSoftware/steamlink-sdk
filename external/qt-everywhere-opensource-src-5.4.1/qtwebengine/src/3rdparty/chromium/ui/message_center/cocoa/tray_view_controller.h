// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_COCOA_TRAY_VIEW_CONTROLLER_H_
#define UI_MESSAGE_CENTER_COCOA_TRAY_VIEW_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include <list>
#include <map>
#include <string>

#include "base/mac/scoped_block.h"
#import "base/mac/scoped_nsobject.h"
#include "base/strings/string16.h"
#include "ui/message_center/message_center_export.h"

@class HoverImageButton;
@class MCNotificationController;
@class MCSettingsController;

namespace message_center {
class MessageCenter;
}

@class HoverImageButton;
@class MCClipView;

namespace message_center {
typedef void(^TrayAnimationEndedCallback)();
}

// The view controller responsible for the content of the message center tray
// UI. This hosts a scroll view of all the notifications, as well as buttons
// to enter quiet mode and the settings panel.
MESSAGE_CENTER_EXPORT
@interface MCTrayViewController : NSViewController<NSAnimationDelegate> {
 @private
  // Controller of the notifications, where action messages are forwarded. Weak.
  message_center::MessageCenter* messageCenter_;

  // The back button shown while the settings are open.
  base::scoped_nsobject<HoverImageButton> backButton_;

  // The "Notifications" label at the top.
  base::scoped_nsobject<NSTextField> title_;

  // The 1px horizontal divider between the scroll view and the title bar.
  base::scoped_nsobject<NSBox> divider_;

  // The "Nothing to see here" label in an empty message center.
  base::scoped_nsobject<NSTextField> emptyDescription_;

  // The scroll view that contains all the notifications in its documentView.
  base::scoped_nsobject<NSScrollView> scrollView_;

  // The clip view that manages how scrollView_'s documentView is clipped.
  base::scoped_nsobject<MCClipView> clipView_;

  // Array of MCNotificationController objects, which the array owns.
  base::scoped_nsobject<NSMutableArray> notifications_;

  // Map of notification IDs to weak pointers of the view controllers in
  // |notifications_|.
  std::map<std::string, MCNotificationController*> notificationsMap_;

  // The pause button that enters quiet mode.
  base::scoped_nsobject<HoverImageButton> pauseButton_;

  // The clear all notifications button. Hidden when there are no notifications.
  base::scoped_nsobject<HoverImageButton> clearAllButton_;

  // The settings button that shows the settings UI.
  base::scoped_nsobject<HoverImageButton> settingsButton_;

  // Array of MCNotificationController objects pending removal by the user.
  // The object is owned by the array.
  base::scoped_nsobject<NSMutableArray> notificationsPendingRemoval_;

  // Used to animate multiple notifications simultaneously when they're being
  // removed or repositioned.
  base::scoped_nsobject<NSViewAnimation> animation_;

  // The controller of the settings view. Only set while the view is open.
  base::scoped_nsobject<MCSettingsController> settingsController_;

  // The flag which is set when the notification removal animation is still
  // in progress and the user clicks "Clear All" button. The clear-all animation
  // will be delayed till the existing animation completes.
  BOOL clearAllDelayed_;

  // The flag which is set when the clear-all animation is in progress.
  BOOL clearAllInProgress_;

  // List of weak pointers of the view controllers that are visible in the
  // scroll view and waiting to slide off one by one when the user clicks
  // "Clear All" button.
  std::list<MCNotificationController*> visibleNotificationsPendingClear_;

  // Array of NSViewAnimation objects, which the array owns.
  base::scoped_nsobject<NSMutableArray> clearAllAnimations_;

  // The duration of the bounds animation, in the number of seconds.
  NSTimeInterval animationDuration_;

  // The delay to start animating clearing next notification, in the number of
  // seconds.
  NSTimeInterval animateClearingNextNotificationDelay_;

  // For testing only. If set, the callback will be called when the animation
  // ends.
  base::mac::ScopedBlock<message_center::TrayAnimationEndedCallback>
      testingAnimationEndedCallback_;
}

// The title that is displayed at the top of the message center tray.
@property(copy, nonatomic) NSString* trayTitle;

// Designated initializer.
- (id)initWithMessageCenter:(message_center::MessageCenter*)messageCenter;

// Called when the window is being closed.
- (void)onWindowClosing;

// Callback for when the MessageCenter model changes.
- (void)onMessageCenterTrayChanged;

// Action for the quiet mode button.
- (void)toggleQuietMode:(id)sender;

// Action for the clear all button.
- (void)clearAllNotifications:(id)sender;

// Action for the settings button.
- (void)showSettings:(id)sender;

// Updates the settings dialog in response to contents change due to something
// like selecting a different profile.
- (void)updateSettings;

// Hides the settings dialog if it's open.
- (void)showMessages:(id)sender;

// Cleans up settings data structures.  Called when messages are shown and when
// closing the center directly from the settings.
- (void)cleanupSettings;

// Scroll to the topmost notification in the tray.
- (void)scrollToTop;

// Returns true if an animation is being played.
- (BOOL)isAnimating;

// Returns the maximum height of the client area of the notifications tray.
+ (CGFloat)maxTrayClientHeight;

// Returns the width of the notifications tray.
+ (CGFloat)trayWidth;

@end

// Testing API /////////////////////////////////////////////////////////////////

@interface MCTrayViewController (TestingAPI)
- (NSBox*)divider;
- (NSTextField*)emptyDescription;
- (NSScrollView*)scrollView;
- (HoverImageButton*)pauseButton;
- (HoverImageButton*)clearAllButton;

// Setter for changing the animation duration. The testing code could set it
// to a very small value to expedite the test running.
- (void)setAnimationDuration:(NSTimeInterval)duration;

// Setter for changing the clear-all animation delay. The testing code could set
// it to a very small value to expedite the test running.
- (void)setAnimateClearingNextNotificationDelay:(NSTimeInterval)delay;

// Setter for testingAnimationEndedCallback_. The testing code could set it
// to get called back when the animation ends.
- (void)setAnimationEndedCallback:
    (message_center::TrayAnimationEndedCallback)callback;
@end

#endif  // UI_MESSAGE_CENTER_COCOA_TRAY_VIEW_CONTROLLER_H_
