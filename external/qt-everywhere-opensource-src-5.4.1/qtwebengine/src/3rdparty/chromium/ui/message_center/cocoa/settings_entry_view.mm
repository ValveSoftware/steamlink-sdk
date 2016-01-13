// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/message_center/cocoa/settings_entry_view.h"

#include <algorithm>

#include "base/strings/sys_string_conversions.h"
#include "grit/ui_resources.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/resource/resource_bundle.h"
#import "ui/message_center/cocoa/settings_controller.h"
#include "ui/message_center/message_center_style.h"

using message_center::settings::kEntryIconSize;
using message_center::settings::kInternalHorizontalSpacing;

// Size of the widget rendered for us by Cocoa.
const int kCocoaCheckboxSize = 14;

// Intrinsic padding pixels out of our control.
// Cocoa gives the checkmark some blank space on either side.
const int kIntrinsicCheckmarkLeftPadding = 2;
const int kIntrinsicCheckmarkRightPadding = 4;
// Labels have a bit of whitespace to the left, which can throw
// off measurements.
const int kIntrinsicTextLeftPadding = 1;

// The learn more image is bigger than the actual size of the learn more
// pixels, this represents the difference.
const int kIntrinsicLearnMorePadding = 2;

// Corrected padding values used in layout.
// This computes the amout of padding based on the area reserved for the
// checkbox and the actual checkbox size in pixels.
const int kCheckmarkPaddingNecessary =
    (message_center::settings::kCheckboxSizeWithPadding - kCocoaCheckboxSize) /
    2;

// These represent the additional padding that we must give the checkmark
// control based on the required padding and the intrinsic padding.
const int kCorrectedCheckmarkLeftPadding =
    kCheckmarkPaddingNecessary - kIntrinsicCheckmarkLeftPadding;
const int kCorrectedCheckmarkRightPadding =
    kCheckmarkPaddingNecessary + kInternalHorizontalSpacing -
    kIntrinsicCheckmarkRightPadding;

// The amount of space we want, based on the spec and the intrinsic text space
// included by Cocoa.
const int kCorrectedIconTextPadding =
    kInternalHorizontalSpacing - kIntrinsicTextLeftPadding;

// We want a certain amount of space to the right of the learn more button,
// this metric incorporates the intrinsic learn more blank space to compute it.
const int kCorrectedEntryRightPadding =
    kInternalHorizontalSpacing - kIntrinsicLearnMorePadding;

////////////////////////////////////////////////////////////////////////////////

@interface MCSettingsButton : NSButton
@end

@implementation MCSettingsButton
// drawRect: needs to fill the button with a background, otherwise we don't get
// subpixel antialiasing.
- (void)drawRect:(NSRect)dirtyRect {
  NSColor* color = gfx::SkColorToCalibratedNSColor(
      message_center::kMessageCenterBackgroundColor);
  [color set];
  NSRectFill(dirtyRect);
  [super drawRect:dirtyRect];
}
@end

@interface MCSettingsButtonCell : NSButtonCell {
  // A checkbox's regular image is the checkmark image. This additional image
  // is used for the favicon or app icon shown next to the checkmark.
  base::scoped_nsobject<NSImage> extraImage_;
}
- (void)setExtraImage:(NSImage*)extraImage;
@end

@implementation MCSettingsButtonCell
- (BOOL)isOpaque {
  return YES;
}

- (void)setExtraImage:(NSImage*)extraImage {
  extraImage_.reset([extraImage retain]);
}

- (NSRect)drawTitle:(NSAttributedString*)title
          withFrame:(NSRect)frame
             inView:(NSView*)controlView {
  CGFloat inset = kCorrectedCheckmarkRightPadding;
  // drawTitle:withFrame:inView: draws the checkmark image. Draw the extra
  // image as part of the checkbox's text.
  if (extraImage_) {
    NSRect imageRect = frame;
    imageRect.origin.x += inset;
    // Center the image vertically.
    if (NSHeight(frame) > kEntryIconSize)
      imageRect.origin.y += (NSHeight(frame) - kEntryIconSize) / 2;
    imageRect.size = NSMakeSize(kEntryIconSize, kEntryIconSize);
    [extraImage_ drawInRect:imageRect
                   fromRect:NSZeroRect
                  operation:NSCompositeSourceOver
                   fraction:1.0
             respectFlipped:YES
                      hints:nil];

    inset += kEntryIconSize + kCorrectedIconTextPadding;
  }
  frame.origin.x += inset;
  frame.size.width -= inset;
  return [super drawTitle:title withFrame:frame inView:controlView];
}

- (NSSize)cellSizeForBounds:(NSRect)aRect {
  NSSize size = [super cellSizeForBounds:aRect];
  size.width += kCorrectedCheckmarkRightPadding;
  if (extraImage_) {
    size.width += kEntryIconSize + kCorrectedIconTextPadding;
    size.height = std::max(static_cast<CGFloat>(kEntryIconSize), size.height);
  }
  return size;
}

- (NSUInteger)hitTestForEvent:(NSEvent*)event
                       inRect:(NSRect)cellFrame
                       ofView:(NSView*)controlView {
  NSUInteger result =
      [super hitTestForEvent:event inRect:cellFrame ofView:controlView];
  if (result == NSCellHitNone) {
    // The default button cell does hit testing on the attributed string. Since
    // this cell draws additional spacing and an icon in front of the string,
    // tweak the hit testing result.
    NSPoint point =
        [controlView convertPoint:[event locationInWindow] fromView:nil];

    NSRect rect = [self titleRectForBounds:[controlView bounds]];
    rect.size = [[self attributedTitle] size];
    rect.size.width += kCorrectedCheckmarkRightPadding;

    if (extraImage_)
      rect.size.width += kEntryIconSize + kCorrectedIconTextPadding;

    if (NSPointInRect(point, rect))
      result = NSCellHitContentArea | NSCellHitTrackableArea;
  }
  return result;
}
@end

@implementation MCSettingsEntryView
- (id)initWithController:(MCSettingsController*)controller
                notifier:(message_center::Notifier*)notifier
                   frame:(NSRect)frame
            hasSeparator:(BOOL)hasSeparator {
  if ((self = [super initWithFrame:frame])) {
    [self setBoxType:NSBoxCustom];
    [self setBorderType:NSNoBorder];
    [self setTitlePosition:NSNoTitle];
    [self setContentViewMargins:NSZeroSize];

    hasSeparator_ = hasSeparator;
    controller_ = controller;
    notifier_ = notifier;
    if (!notifier->icon.IsEmpty())
      notifierIcon_.reset(notifier->icon.CopyNSImage());
    [self layout];
  }
  return self;
}

- (void)setNotifierIcon:(NSImage*)notifierIcon {
  notifierIcon_.reset([notifierIcon retain]);
  [self layout];
}

- (NSButton*)checkbox {
  return checkbox_;
}

- (void)layout {
  BOOL hasLearnMore =
      [controller_ notifierHasAdvancedSettings:notifier_->notifier_id];

  // Now calculate the space available for the checkbox button.
  NSRect checkboxFrame = [self bounds];
  checkboxFrame.origin.x += kCorrectedCheckmarkLeftPadding;
  checkboxFrame.size.width -=
      kCorrectedCheckmarkLeftPadding + kCorrectedEntryRightPadding;

  NSRect learnMoreFrame =
      NSMakeRect(checkboxFrame.origin.x + checkboxFrame.size.width,
                 checkboxFrame.origin.y,
                 0,
                 checkboxFrame.size.height);

  // Initially place the learn more button right-aligned.
  if (hasLearnMore) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    NSImage* defaultImage =
        rb.GetNativeImageNamed(IDR_NOTIFICATION_ADVANCED_SETTINGS).ToNSImage();
    NSSize defaultImageSize = [defaultImage size];
    learnMoreFrame.size.width = defaultImageSize.width;
    learnMoreFrame.origin.x -= defaultImageSize.width;

    // May need to center the image if it's shorter than the entry.
    if (defaultImageSize.height < learnMoreFrame.size.height) {
      learnMoreFrame.origin.y +=
          (learnMoreFrame.size.height - defaultImageSize.height) / 2;
      learnMoreFrame.size.height = defaultImageSize.height;
    }

    // Since we have an image then we need to ensure that the text from the
    // checkbox doesn't overlap with the learn more image.
    checkboxFrame.size.width -=
        kCorrectedIconTextPadding + learnMoreFrame.size.width;

    if (!learnMoreButton_.get()) {
      learnMoreButton_.reset(
          [[HoverImageButton alloc] initWithFrame:learnMoreFrame]);
      [self addSubview:learnMoreButton_];
    } else {
      [learnMoreButton_ setFrame:learnMoreFrame];
    }
    [learnMoreButton_ setDefaultImage:defaultImage];
    [learnMoreButton_ setHoverImage:rb.GetNativeImageNamed(
        IDR_NOTIFICATION_ADVANCED_SETTINGS_HOVER).ToNSImage()];
    [learnMoreButton_ setPressedImage:rb.GetNativeImageNamed(
        IDR_NOTIFICATION_ADVANCED_SETTINGS_PRESSED).ToNSImage()];
    [learnMoreButton_ setBordered:NO];
    [learnMoreButton_ setTarget:self];
    [learnMoreButton_ setAction:@selector(learnMoreClicked:)];
  }

  if (!checkbox_.get()) {
    checkbox_.reset([[MCSettingsButton alloc] initWithFrame:checkboxFrame]);
    [self addSubview:checkbox_];
  } else {
    [checkbox_ setFrame:checkboxFrame];
  }

  base::scoped_nsobject<MCSettingsButtonCell> cell([[MCSettingsButtonCell alloc]
      initTextCell:base::SysUTF16ToNSString(notifier_->name)]);
  if ([notifierIcon_ isValid])
    [cell setExtraImage:notifierIcon_];

  [checkbox_ setCell:cell];
  [checkbox_ setButtonType:NSSwitchButton];
  [checkbox_ setState:notifier_->enabled ? NSOnState : NSOffState];
  [checkbox_ setTarget:self];
  [checkbox_ setAction:@selector(checkboxClicked:)];

  if (hasSeparator_) {
    NSRect separatorRect = [self bounds];
    separatorRect.size.height = 1;
    if (!separator_.get()) {
      separator_.reset([[NSBox alloc] initWithFrame:separatorRect]);
      [separator_ setBoxType:NSBoxCustom];
      [separator_ setBorderType:NSLineBorder];
      [separator_ setBorderColor:gfx::SkColorToCalibratedNSColor(
          message_center::settings::kEntrySeparatorColor)];
      [separator_ setTitlePosition:NSNoTitle];
      [separator_ setContentViewMargins:NSZeroSize];
      [self addSubview:separator_];
    } else {
      [separator_ setFrame:separatorRect];
    }
  }
}

- (void)checkboxClicked:(id)sender {
  BOOL enabled = [sender state] == NSOnState;
  [controller_ setSettingsNotifier:notifier_ enabled:enabled];
}

- (void)learnMoreClicked:(id)sender {
  [controller_ learnMoreClicked:notifier_];
}

// Testing API /////////////////////////////////////////////////////////////////

- (void)clickLearnMore {
  [learnMoreButton_ performClick:nil];
}

@end

///////////////////////////////////////////////////////////////////////////////
