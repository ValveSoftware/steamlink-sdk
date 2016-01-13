// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/message_center/cocoa/settings_controller.h"

#include <algorithm>

#include "base/mac/foundation_util.h"
#import "base/mac/scoped_nsobject.h"
#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"
#include "grit/ui_strings.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/l10n/l10n_util.h"
#import "ui/message_center/cocoa/opaque_views.h"
#import "ui/message_center/cocoa/settings_entry_view.h"
#import "ui/message_center/cocoa/tray_view_controller.h"
#include "ui/message_center/message_center_style.h"

using message_center::settings::kHorizontalMargin;
using message_center::settings::kEntryHeight;

// Intrinsic padding pixels out of our control.
const int kIntrinsicHeaderTextTopPadding = 3;
const int kIntrinsicSubheaderTextTopPadding = 5;
const int kIntrinsicSubheaderTextBottomPadding = 3;
const int kIntrinsicDropDownVerticalPadding = 2;
const int kIntrinsicDropDownHorizontalPadding = 3;

// Corrected padding values used in layout.
// Calculated additional blank space above the header text, including
// the intrinsic blank space above the header label.
const int kCorrectedHeaderTextTopPadding =
    message_center::settings::kTopMargin - kIntrinsicHeaderTextTopPadding;

// Calculated additional blank space above the subheader text, including
// the intrinsic blank space above the subheader label.
const int kCorrectedSubheaderTextTopPadding =
    message_center::settings::kTitleToDescriptionSpace -
    kIntrinsicSubheaderTextTopPadding;

// Calcoulated additional vertical padding for the drop-down, including the
// blank space included with the drop-down control.
const int kCorrectedDropDownTopPadding =
    message_center::settings::kDescriptionToSwitcherSpace -
    kIntrinsicDropDownVerticalPadding - kIntrinsicSubheaderTextBottomPadding;

// Calculated additional horizontal blank space for the drop down, including
// the blank space included with the drop-down control.
const int kCorrectedDropDownMargin =
    kHorizontalMargin - kIntrinsicDropDownHorizontalPadding;

@interface MCSettingsController (Private)
// Sets the icon on the checkbox corresponding to |notifiers_[index]|.
- (void)setIcon:(NSImage*)icon forNotifierIndex:(size_t)index;

- (void)setIcon:(NSImage*)icon
    forNotifierId:(const message_center::NotifierId&)id;

// Returns the NSButton corresponding to the checkbox for |notifiers_[index]|.
- (MCSettingsEntryView*)entryForNotifierAtIndex:(size_t)index;

// Update the contents view.
- (void)updateView;

// Handler for the notifier group dropdown menu.
- (void)notifierGroupSelectionChanged:(id)sender;

@end

namespace message_center {

NotifierSettingsObserverMac::~NotifierSettingsObserverMac() {}

void NotifierSettingsObserverMac::UpdateIconImage(const NotifierId& notifier_id,
                                                  const gfx::Image& icon) {
  [settings_controller_ setIcon:icon.AsNSImage() forNotifierId:notifier_id];
}

void NotifierSettingsObserverMac::NotifierGroupChanged() {
  [settings_controller_ updateView];
}

void NotifierSettingsObserverMac::NotifierEnabledChanged(
    const NotifierId& notifier_id, bool enabled) {}

}  // namespace message_center

@implementation MCSettingsController

- (id)initWithProvider:(message_center::NotifierSettingsProvider*)provider
    trayViewController:(MCTrayViewController*)trayViewController {
  if ((self = [super initWithNibName:nil bundle:nil])) {
    observer_.reset(new message_center::NotifierSettingsObserverMac(self));
    provider_ = provider;
    trayViewController_ = trayViewController;
    provider_->AddObserver(observer_.get());
  }
  return self;
}

- (void)dealloc {
  provider_->RemoveObserver(observer_.get());
  provider_->OnNotifierSettingsClosing();
  STLDeleteElements(&notifiers_);
  [super dealloc];
}

- (NSTextField*)newLabelWithFrame:(NSRect)frame {
  NSColor* color = gfx::SkColorToCalibratedNSColor(
      message_center::kMessageCenterBackgroundColor);
  MCTextField* label =
      [[MCTextField alloc] initWithFrame:frame backgroundColor:color];

  return label;
}

- (void)updateView {
  notifiers_.clear();
  [trayViewController_ updateSettings];
}

- (void)loadView {
  DCHECK(notifiers_.empty());
  provider_->GetNotifierList(&notifiers_);
  CGFloat maxHeight = [MCTrayViewController maxTrayClientHeight];

  // Container view.
  NSRect fullFrame =
      NSMakeRect(0, 0, [MCTrayViewController trayWidth], maxHeight);
  base::scoped_nsobject<NSBox> view([[NSBox alloc] initWithFrame:fullFrame]);
  [view setBorderType:NSNoBorder];
  [view setBoxType:NSBoxCustom];
  [view setContentViewMargins:NSZeroSize];
  [view setFillColor:gfx::SkColorToCalibratedNSColor(
      message_center::kMessageCenterBackgroundColor)];
  [view setTitlePosition:NSNoTitle];
  [self setView:view];

  // "Settings" text.
  NSRect headerFrame = NSMakeRect(kHorizontalMargin,
                                  kHorizontalMargin,
                                  NSWidth(fullFrame),
                                  NSHeight(fullFrame));
  settingsText_.reset([self newLabelWithFrame:headerFrame]);
  [settingsText_ setTextColor:
          gfx::SkColorToCalibratedNSColor(message_center::kRegularTextColor)];
  [settingsText_
      setFont:[NSFont messageFontOfSize:message_center::kTitleFontSize]];

  [settingsText_ setStringValue:
          l10n_util::GetNSString(IDS_MESSAGE_CENTER_SETTINGS_BUTTON_LABEL)];
  [settingsText_ sizeToFit];
  headerFrame = [settingsText_ frame];
  headerFrame.origin.y = NSMaxY(fullFrame) - kCorrectedHeaderTextTopPadding -
                         NSHeight(headerFrame);
  [[self view] addSubview:settingsText_];

  // Subheader.
  NSRect subheaderFrame = NSMakeRect(kHorizontalMargin,
                                     kHorizontalMargin,
                                     NSWidth(fullFrame),
                                     NSHeight(fullFrame));
  detailsText_.reset([self newLabelWithFrame:subheaderFrame]);
  [detailsText_ setTextColor:
      gfx::SkColorToCalibratedNSColor(message_center::kDimTextColor)];
  [detailsText_
      setFont:[NSFont messageFontOfSize:message_center::kMessageFontSize]];

  size_t groupCount = provider_->GetNotifierGroupCount();
  [detailsText_ setStringValue:l10n_util::GetNSString(
      groupCount > 1 ? IDS_MESSAGE_CENTER_SETTINGS_DESCRIPTION_MULTIUSER
                     : IDS_MESSAGE_CENTER_SETTINGS_DIALOG_DESCRIPTION)];
  [detailsText_ sizeToFit];
  subheaderFrame = [detailsText_ frame];
  subheaderFrame.origin.y =
      NSMinY(headerFrame) - kCorrectedSubheaderTextTopPadding -
      NSHeight(subheaderFrame);
  [[self view] addSubview:detailsText_];

  // Profile switcher is only needed for more than one profile.
  NSRect dropDownButtonFrame = subheaderFrame;
  if (groupCount > 1) {
    dropDownButtonFrame = NSMakeRect(kCorrectedDropDownMargin,
                                     kHorizontalMargin,
                                     NSWidth(fullFrame),
                                     NSHeight(fullFrame));
    groupDropDownButton_.reset(
        [[MCDropDown alloc] initWithFrame:dropDownButtonFrame pullsDown:YES]);
    [groupDropDownButton_
        setBackgroundColor:gfx::SkColorToCalibratedNSColor(
                               message_center::kMessageCenterBackgroundColor)];
    [groupDropDownButton_ setAction:@selector(notifierGroupSelectionChanged:)];
    [groupDropDownButton_ setTarget:self];
    // Add a dummy item for pull-down.
    [groupDropDownButton_ addItemWithTitle:@""];
    base::string16 title;
    for (size_t i = 0; i < groupCount; ++i) {
      const message_center::NotifierGroup& group =
          provider_->GetNotifierGroupAt(i);
      base::string16 item =
          group.login_info.empty() ? group.name : group.login_info;
      [groupDropDownButton_ addItemWithTitle:base::SysUTF16ToNSString(item)];
      if (provider_->IsNotifierGroupActiveAt(i)) {
        title = item;
        [[groupDropDownButton_ lastItem] setState:NSOnState];
      }
    }
    [groupDropDownButton_ setTitle:base::SysUTF16ToNSString(title)];
    [groupDropDownButton_ sizeToFit];
    dropDownButtonFrame = [groupDropDownButton_ frame];
    dropDownButtonFrame.origin.y =
        NSMinY(subheaderFrame) - kCorrectedDropDownTopPadding -
        NSHeight(dropDownButtonFrame);
    dropDownButtonFrame.size.width =
        NSWidth(fullFrame) - 2 * kCorrectedDropDownMargin;
    [[self view] addSubview:groupDropDownButton_];
  }

  // Document view for the notifier settings.
  CGFloat y = 0;
  NSRect documentFrame = NSMakeRect(0, 0, NSWidth(fullFrame), 0);
  base::scoped_nsobject<NSView> documentView(
      [[NSView alloc] initWithFrame:documentFrame]);
  int notifierCount = notifiers_.size();
  for (int i = notifierCount - 1; i >= 0; --i) {
    message_center::Notifier* notifier = notifiers_[i];
    // TODO(thakis): Use a custom button cell.
    NSRect frame = NSMakeRect(kHorizontalMargin,
                              y,
                              NSWidth(documentFrame) - kHorizontalMargin * 2,
                              kEntryHeight);

    base::scoped_nsobject<MCSettingsEntryView> entryView(
        [[MCSettingsEntryView alloc]
            initWithController:self
                      notifier:notifier
                         frame:frame
                  hasSeparator:(i != notifierCount - 1)]);
    [documentView addSubview:entryView];
    y += NSHeight(frame);
  }

  documentFrame.size.height = y - kIntrinsicDropDownVerticalPadding;
  [documentView setFrame:documentFrame];

  // Scroll view for the notifier settings.
  NSRect scrollFrame = documentFrame;
  scrollFrame.origin.y = 0;
  CGFloat remainingHeight = NSMinY(dropDownButtonFrame) - NSMinY(scrollFrame);

  if (NSHeight(documentFrame) < remainingHeight) {
    // Everything fits without scrolling.
    CGFloat delta = remainingHeight - NSHeight(documentFrame);
    headerFrame.origin.y -= delta;
    subheaderFrame.origin.y -= delta;
    dropDownButtonFrame.origin.y -= delta;
    fullFrame.size.height -= delta;
  } else {
    scrollFrame.size.height = remainingHeight;
  }

  scrollView_.reset([[NSScrollView alloc] initWithFrame:scrollFrame]);
  [scrollView_ setAutohidesScrollers:YES];
  [scrollView_ setAutoresizingMask:NSViewMinYMargin];
  [scrollView_ setDocumentView:documentView];
  [scrollView_ setDrawsBackground:NO];
  [scrollView_ setHasHorizontalScroller:NO];
  [scrollView_ setHasVerticalScroller:YES];

  // Scroll to top.
  NSPoint newScrollOrigin =
      NSMakePoint(0.0,
                  NSMaxY([[scrollView_ documentView] frame]) -
                      NSHeight([[scrollView_ contentView] bounds]));
  [[scrollView_ documentView] scrollPoint:newScrollOrigin];

  // Set final sizes.
  [[self view] setFrame:fullFrame];
  [[self view] addSubview:scrollView_];
  [settingsText_ setFrame:headerFrame];
  [detailsText_ setFrame:subheaderFrame];
  [groupDropDownButton_ setFrame:dropDownButtonFrame];
}

- (void)setSettingsNotifier:(message_center::Notifier*)notifier
                    enabled:(BOOL)enabled {
  provider_->SetNotifierEnabled(*notifier, enabled);
}

- (void)learnMoreClicked:(message_center::Notifier*)notifier {
  provider_->OnNotifierAdvancedSettingsRequested(notifier->notifier_id, NULL);
}

// Testing API /////////////////////////////////////////////////////////////////

- (NSPopUpButton*)groupDropDownButton {
  return groupDropDownButton_;
}

- (NSScrollView*)scrollView {
  return scrollView_;
}

// Private API /////////////////////////////////////////////////////////////////

- (void)setIcon:(NSImage*)icon forNotifierIndex:(size_t)index {
  MCSettingsEntryView* entry = [self entryForNotifierAtIndex:index];
  [entry setNotifierIcon:icon];
}

- (void)setIcon:(NSImage*)icon
    forNotifierId:(const message_center::NotifierId&)id {
  for (size_t i = 0; i < notifiers_.size(); ++i) {
    if (notifiers_[i]->notifier_id == id) {
      [self setIcon:icon forNotifierIndex:i];
      return;
    }
  }
}

- (MCSettingsEntryView*)entryForNotifierAtIndex:(size_t)index {
  NSArray* subviews = [[scrollView_ documentView] subviews];
  // The checkboxes are in bottom-top order, the checkbox for notifiers_[0] is
  // last.
  DCHECK_LT(notifiers_.size() - 1 - index, [subviews count]);
  NSView* view = [subviews objectAtIndex:notifiers_.size() - 1 - index];
  return base::mac::ObjCCastStrict<MCSettingsEntryView>(view);
}

- (void)notifierGroupSelectionChanged:(id)sender {
  DCHECK_EQ(groupDropDownButton_.get(), sender);
  NSPopUpButton* button = static_cast<NSPopUpButton*>(sender);
  // The first item is a dummy item.
  provider_->SwitchToNotifierGroup([button indexOfSelectedItem] - 1);
}

- (BOOL)notifierHasAdvancedSettings:(const message_center::NotifierId&)id {
  return provider_->NotifierHasAdvancedSettings(id);
}

@end
