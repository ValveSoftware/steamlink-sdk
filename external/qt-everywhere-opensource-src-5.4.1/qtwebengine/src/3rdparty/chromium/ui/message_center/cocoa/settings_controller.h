// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_COCOA_SETTINGS_CONTROLLER_H_
#define UI_MESSAGE_CENTER_COCOA_SETTINGS_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#import "ui/message_center/cocoa/opaque_views.h"
#import "ui/message_center/cocoa/settings_entry_view.h"
#include "ui/message_center/message_center_export.h"
#include "ui/message_center/notifier_settings.h"

@class MCSettingsController;
@class MCTrayViewController;

namespace message_center {

// Bridge class between C++ and Cocoa world.
class NotifierSettingsObserverMac : public NotifierSettingsObserver {
 public:
  NotifierSettingsObserverMac(MCSettingsController* settings_controller)
      : settings_controller_(settings_controller) {}
  virtual ~NotifierSettingsObserverMac();

  // Overridden from NotifierSettingsObserver:
  virtual void UpdateIconImage(const NotifierId& notifier_id,
                               const gfx::Image& icon) OVERRIDE;
  virtual void NotifierGroupChanged() OVERRIDE;
  virtual void NotifierEnabledChanged(const NotifierId& notifier_id,
                                      bool enabled) OVERRIDE;

 private:
  MCSettingsController* settings_controller_;  // weak, owns this

  DISALLOW_COPY_AND_ASSIGN(NotifierSettingsObserverMac);
};

}  // namespace message_center

// The view controller responsible for the settings sheet in the center.
MESSAGE_CENTER_EXPORT
@interface MCSettingsController : NSViewController {
 @private
  scoped_ptr<message_center::NotifierSettingsObserverMac> observer_;
  message_center::NotifierSettingsProvider* provider_;
  MCTrayViewController* trayViewController_;  // Weak. Owns us.

  // The "Settings" text at the top.
  base::scoped_nsobject<NSTextField> settingsText_;

  // The smaller text below the "Settings" text.
  base::scoped_nsobject<NSTextField> detailsText_;

  // The profile switcher.
  base::scoped_nsobject<MCDropDown> groupDropDownButton_;

  // Container for all the checkboxes.
  base::scoped_nsobject<NSScrollView> scrollView_;

  std::vector<message_center::Notifier*> notifiers_;
}

// Designated initializer.
- (id)initWithProvider:(message_center::NotifierSettingsProvider*)provider
    trayViewController:(MCTrayViewController*)trayViewController;

// Returns whether |provider_| has an advanced settings handler for the given
// notifier; i.e. we should show the "Learn More" button.
- (BOOL)notifierHasAdvancedSettings:(const message_center::NotifierId&)id;

// Handler when a checkbox is enabled/disabled.
- (void)setSettingsNotifier:(message_center::Notifier*)notifier
                    enabled:(BOOL)enabled;

// Handler when the learn more link is clicked.
- (void)learnMoreClicked:(message_center::Notifier*)notifier;

@end

// Testing API /////////////////////////////////////////////////////////////////

@interface MCSettingsController (TestingAPI)
- (NSPopUpButton*)groupDropDownButton;
- (NSScrollView*)scrollView;
@end

#endif  // UI_MESSAGE_CENTER_COCOA_SETTINGS_CONTROLLER_H_
