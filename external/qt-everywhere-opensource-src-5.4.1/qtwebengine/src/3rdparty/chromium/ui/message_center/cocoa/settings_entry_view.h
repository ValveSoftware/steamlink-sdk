// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_COCOA_SETTINGS_ENTRY_VIEW_H_
#define UI_MESSAGE_CENTER_COCOA_SETTINGS_ENTRY_VIEW_H_

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#import "ui/base/cocoa/hover_image_button.h"
#include "ui/message_center/notifier_settings.h"

@class MCSettingsController;

// The view that renders individual notifiers in the settings sheet.  This
// includes an enable/disable checkbox, icon, name, and learn more button,
// and an optional horizontal separator line.
@interface MCSettingsEntryView : NSBox {
 @private
  // Weak. Owns us.
  MCSettingsController* controller_;

  // Weak, owned by MCSettingsController.
  message_center::Notifier* notifier_;

  // The image that will be displayed next to the notifier name, loaded
  // asynchronously.
  base::scoped_nsobject<NSImage> notifierIcon_;

  // The button that can be displayed after the notifier name, that when
  // clicked on causes an event to be fired for more information.
  base::scoped_nsobject<HoverImageButton> learnMoreButton_;

  // The button that contains the label, notifier icon and checkbox.
  base::scoped_nsobject<NSButton> checkbox_;

  // The border on the bottom of the view.
  BOOL hasSeparator_;
  base::scoped_nsobject<NSBox> separator_;
}

- (id)initWithController:(MCSettingsController*)controller
                notifier:(message_center::Notifier*)notifier
                   frame:(NSRect)frame
            hasSeparator:(BOOL)hasSeparator;

- (void)setNotifierIcon:(NSImage*)notifierIcon;

- (NSButton*)checkbox;

@end

@interface MCSettingsEntryView (TestingAPI)
- (void)clickLearnMore;
@end

#endif  // UI_MESSAGE_CENTER_COCOA_SETTINGS_ENTRY_VIEW_H_
