/*
 * Copyright (C) 2008, 2010, 2011, 2012 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "platform/mac/ThemeMac.h"

#import <Carbon/Carbon.h>
#import "platform/graphics/GraphicsContextStateSaver.h"
#import "platform/mac/BlockExceptions.h"
#import "platform/mac/LocalCurrentGraphicsContext.h"
#import "platform/mac/VersionUtilMac.h"
#import "platform/mac/WebCoreNSCellExtras.h"
#import "platform/scroll/ScrollableArea.h"
#include "wtf/StdLibExtras.h"

// This is a view whose sole purpose is to tell AppKit that it's flipped.
@interface BlinkFlippedControl : NSControl
@end

@implementation BlinkFlippedControl

- (BOOL)isFlipped {
  return YES;
}

- (NSText*)currentEditor {
  return nil;
}

- (BOOL)_automaticFocusRingDisabled {
  return YES;
}

@end

namespace blink {

Theme* platformTheme() {
  DEFINE_STATIC_LOCAL(ThemeMac, themeMac, ());
  return &themeMac;
}

// Helper functions used by a bunch of different control parts.

static NSControlSize controlSizeForFont(
    const FontDescription& fontDescription) {
  int fontSize = fontDescription.computedPixelSize();
  if (fontSize >= 16)
    return NSRegularControlSize;
  if (fontSize >= 11)
    return NSSmallControlSize;
  return NSMiniControlSize;
}

static LengthSize sizeFromNSControlSize(NSControlSize nsControlSize,
                                        const LengthSize& zoomedSize,
                                        float zoomFactor,
                                        const IntSize* sizes) {
  IntSize controlSize = sizes[nsControlSize];
  if (zoomFactor != 1.0f)
    controlSize = IntSize(controlSize.width() * zoomFactor,
                          controlSize.height() * zoomFactor);
  LengthSize result = zoomedSize;
  if (zoomedSize.width().isIntrinsicOrAuto() && controlSize.width() > 0)
    result.setWidth(Length(controlSize.width(), Fixed));
  if (zoomedSize.height().isIntrinsicOrAuto() && controlSize.height() > 0)
    result.setHeight(Length(controlSize.height(), Fixed));
  return result;
}

static LengthSize sizeFromFont(const FontDescription& fontDescription,
                               const LengthSize& zoomedSize,
                               float zoomFactor,
                               const IntSize* sizes) {
  return sizeFromNSControlSize(controlSizeForFont(fontDescription), zoomedSize,
                               zoomFactor, sizes);
}

NSControlSize ThemeMac::controlSizeFromPixelSize(const IntSize* sizes,
                                                 const IntSize& minZoomedSize,
                                                 float zoomFactor) {
  if (minZoomedSize.width() >=
          static_cast<int>(sizes[NSRegularControlSize].width() * zoomFactor) &&
      minZoomedSize.height() >=
          static_cast<int>(sizes[NSRegularControlSize].height() * zoomFactor))
    return NSRegularControlSize;
  if (minZoomedSize.width() >=
          static_cast<int>(sizes[NSSmallControlSize].width() * zoomFactor) &&
      minZoomedSize.height() >=
          static_cast<int>(sizes[NSSmallControlSize].height() * zoomFactor))
    return NSSmallControlSize;
  return NSMiniControlSize;
}

static void setControlSize(NSCell* cell,
                           const IntSize* sizes,
                           const IntSize& minZoomedSize,
                           float zoomFactor) {
  ControlSize size =
      ThemeMac::controlSizeFromPixelSize(sizes, minZoomedSize, zoomFactor);
  // Only update if we have to, since AppKit does work even if the size is the
  // same.
  if (size != [cell controlSize])
    [cell setControlSize:(NSControlSize)size];
}

static void updateStates(NSCell* cell, ControlStates states) {
  // Hover state is not supported by Aqua.

  // Pressed state
  bool oldPressed = [cell isHighlighted];
  bool pressed = states & PressedControlState;
  if (pressed != oldPressed)
    [cell setHighlighted:pressed];

  // Enabled state
  bool oldEnabled = [cell isEnabled];
  bool enabled = states & EnabledControlState;
  if (enabled != oldEnabled)
    [cell setEnabled:enabled];

  // Checked and Indeterminate
  bool oldIndeterminate = [cell state] == NSMixedState;
  bool indeterminate = (states & IndeterminateControlState);
  bool checked = states & CheckedControlState;
  bool oldChecked = [cell state] == NSOnState;
  if (oldIndeterminate != indeterminate || checked != oldChecked)
    [cell setState:indeterminate ? NSMixedState
                                 : (checked ? NSOnState : NSOffState)];

  // Window inactive state does not need to be checked explicitly, since we
  // paint parented to a view in a window whose key state can be detected.
}

// Return a fake NSView whose sole purpose is to tell AppKit that it's flipped.
NSView* ThemeMac::ensuredView(ScrollableArea* scrollableArea) {
  // Use a fake flipped view.
  static NSView* flippedView = [[BlinkFlippedControl alloc] init];
  [flippedView setFrameSize:NSSizeFromCGSize(scrollableArea->contentsSize())];

  return flippedView;
}

// static
IntRect ThemeMac::inflateRect(const IntRect& zoomedRect,
                              const IntSize& zoomedSize,
                              const int* margins,
                              float zoomFactor) {
  // Only do the inflation if the available width/height are too small.
  // Otherwise try to fit the glow/check space into the available box's
  // width/height.
  int widthDelta = zoomedRect.width() -
                   (zoomedSize.width() + margins[LeftMargin] * zoomFactor +
                    margins[RightMargin] * zoomFactor);
  int heightDelta = zoomedRect.height() -
                    (zoomedSize.height() + margins[TopMargin] * zoomFactor +
                     margins[BottomMargin] * zoomFactor);
  IntRect result(zoomedRect);
  if (widthDelta < 0) {
    result.setX(result.x() - margins[LeftMargin] * zoomFactor);
    result.setWidth(result.width() - widthDelta);
  }
  if (heightDelta < 0) {
    result.setY(result.y() - margins[TopMargin] * zoomFactor);
    result.setHeight(result.height() - heightDelta);
  }
  return result;
}

// static
IntRect ThemeMac::inflateRectForAA(const IntRect& rect) {
  const int margin = 2;
  return IntRect(rect.x() - margin, rect.y() - margin,
                 rect.width() + 2 * margin, rect.height() + 2 * margin);
}

// static
IntRect ThemeMac::inflateRectForFocusRing(const IntRect& rect) {
  // Just put a margin of 16 units around the rect. The UI elements that use
  // this don't appropriately scale their focus rings appropriately (e.g, paint
  // pickers), or switch to non-native widgets when scaled (e.g, check boxes
  // and radio buttons).
  const int margin = 16;
  IntRect result;
  result.setX(rect.x() - margin);
  result.setY(rect.y() - margin);
  result.setWidth(rect.width() + 2 * margin);
  result.setHeight(rect.height() + 2 * margin);
  return result;
}

// Checkboxes

const IntSize* ThemeMac::checkboxSizes() {
  static const IntSize sizes[3] = {IntSize(14, 14), IntSize(12, 12),
                                   IntSize(10, 10)};
  return sizes;
}

const int* ThemeMac::checkboxMargins(NSControlSize controlSize) {
  static const int margins[3][4] = {
      {3, 4, 4, 2}, {4, 3, 3, 3}, {4, 3, 3, 3},
  };
  return margins[controlSize];
}

LengthSize ThemeMac::checkboxSize(const FontDescription& fontDescription,
                                  const LengthSize& zoomedSize,
                                  float zoomFactor) {
  // If the width and height are both specified, then we have nothing to do.
  if (!zoomedSize.width().isIntrinsicOrAuto() &&
      !zoomedSize.height().isIntrinsicOrAuto())
    return zoomedSize;

  // Use the font size to determine the intrinsic width of the control.
  return sizeFromFont(fontDescription, zoomedSize, zoomFactor, checkboxSizes());
}

NSButtonCell* ThemeMac::checkbox(ControlStates states,
                                 const IntRect& zoomedRect,
                                 float zoomFactor) {
  static NSButtonCell* checkboxCell;
  if (!checkboxCell) {
    checkboxCell = [[NSButtonCell alloc] init];
    [checkboxCell setButtonType:NSSwitchButton];
    [checkboxCell setTitle:nil];
    [checkboxCell setAllowsMixedState:YES];
    [checkboxCell setFocusRingType:NSFocusRingTypeExterior];
  }

  // Set the control size based off the rectangle we're painting into.
  setControlSize(checkboxCell, checkboxSizes(), zoomedRect.size(), zoomFactor);

  // Update the various states we respond to.
  updateStates(checkboxCell, states);

  return checkboxCell;
}

const IntSize* ThemeMac::radioSizes() {
  static const IntSize sizes[3] = {IntSize(14, 15), IntSize(12, 13),
                                   IntSize(10, 10)};
  return sizes;
}

const int* ThemeMac::radioMargins(NSControlSize controlSize) {
  static const int margins[3][4] = {
      {2, 2, 4, 2}, {3, 2, 3, 2}, {1, 0, 2, 0},
  };
  return margins[controlSize];
}

LengthSize ThemeMac::radioSize(const FontDescription& fontDescription,
                               const LengthSize& zoomedSize,
                               float zoomFactor) {
  // If the width and height are both specified, then we have nothing to do.
  if (!zoomedSize.width().isIntrinsicOrAuto() &&
      !zoomedSize.height().isIntrinsicOrAuto())
    return zoomedSize;

  // Use the font size to determine the intrinsic width of the control.
  return sizeFromFont(fontDescription, zoomedSize, zoomFactor, radioSizes());
}

NSButtonCell* ThemeMac::radio(ControlStates states,
                              const IntRect& zoomedRect,
                              float zoomFactor) {
  static NSButtonCell* radioCell;
  if (!radioCell) {
    radioCell = [[NSButtonCell alloc] init];
    [radioCell setButtonType:NSRadioButton];
    [radioCell setTitle:nil];
    [radioCell setFocusRingType:NSFocusRingTypeExterior];
  }

  // Set the control size based off the rectangle we're painting into.
  setControlSize(radioCell, radioSizes(), zoomedRect.size(), zoomFactor);

  // Update the various states we respond to.
  // Cocoa draws NSMixedState NSRadioButton as NSOnState so we don't want that.
  states &= ~IndeterminateControlState;
  updateStates(radioCell, states);

  return radioCell;
}

// Buttons really only constrain height. They respect width.
const IntSize* ThemeMac::buttonSizes() {
  static const IntSize sizes[3] = {IntSize(0, 21), IntSize(0, 18),
                                   IntSize(0, 15)};
  return sizes;
}

const int* ThemeMac::buttonMargins(NSControlSize controlSize) {
  static const int margins[3][4] = {
      {4, 6, 7, 6}, {4, 5, 6, 5}, {0, 1, 1, 1},
  };
  return margins[controlSize];
}

static void setUpButtonCell(NSButtonCell* cell,
                            ControlPart part,
                            ControlStates states,
                            const IntRect& zoomedRect,
                            float zoomFactor) {
  // Set the control size based off the rectangle we're painting into.
  const IntSize* sizes = ThemeMac::buttonSizes();
  if (part == SquareButtonPart ||
      zoomedRect.height() >
          ThemeMac::buttonSizes()[NSRegularControlSize].height() * zoomFactor) {
    // Use the square button
    if ([cell bezelStyle] != NSShadowlessSquareBezelStyle)
      [cell setBezelStyle:NSShadowlessSquareBezelStyle];
  } else if ([cell bezelStyle] != NSRoundedBezelStyle)
    [cell setBezelStyle:NSRoundedBezelStyle];

  setControlSize(cell, sizes, zoomedRect.size(), zoomFactor);

  // Update the various states we respond to.
  updateStates(cell, states);
}

NSButtonCell* ThemeMac::button(ControlPart part,
                               ControlStates states,
                               const IntRect& zoomedRect,
                               float zoomFactor) {
  static NSButtonCell* cell = nil;
  if (!cell) {
    cell = [[NSButtonCell alloc] init];
    [cell setTitle:nil];
    [cell setButtonType:NSMomentaryPushInButton];
  }
  setUpButtonCell(cell, part, states, zoomedRect, zoomFactor);
  return cell;
}

const IntSize* ThemeMac::stepperSizes() {
  static const IntSize sizes[3] = {IntSize(19, 27), IntSize(15, 22),
                                   IntSize(13, 15)};
  return sizes;
}

// We don't use controlSizeForFont() for steppers because the stepper height
// should be equal to or less than the corresponding text field height,
static NSControlSize stepperControlSizeForFont(
    const FontDescription& fontDescription) {
  int fontSize = fontDescription.computedPixelSize();
  if (fontSize >= 27)
    return NSRegularControlSize;
  if (fontSize >= 22)
    return NSSmallControlSize;
  return NSMiniControlSize;
}

// Theme overrides

int ThemeMac::baselinePositionAdjustment(ControlPart part) const {
  if (part == CheckboxPart || part == RadioPart)
    return -2;
  return Theme::baselinePositionAdjustment(part);
}

FontDescription ThemeMac::controlFont(ControlPart part,
                                      const FontDescription& fontDescription,
                                      float zoomFactor) const {
  switch (part) {
    case PushButtonPart: {
      FontDescription result;
      result.setIsAbsoluteSize(true);
      result.setGenericFamily(FontDescription::SerifFamily);

      NSFont* nsFont = [NSFont
          systemFontOfSize:[NSFont systemFontSizeForControlSize:
                                       controlSizeForFont(fontDescription)]];
      result.firstFamily().setFamily(FontFamilyNames::system_ui);
      result.setComputedSize([nsFont pointSize] * zoomFactor);
      result.setSpecifiedSize([nsFont pointSize] * zoomFactor);
      return result;
    }
    default:
      return Theme::controlFont(part, fontDescription, zoomFactor);
  }
}

LengthSize ThemeMac::controlSize(ControlPart part,
                                 const FontDescription& fontDescription,
                                 const LengthSize& zoomedSize,
                                 float zoomFactor) const {
  switch (part) {
    case CheckboxPart:
      return checkboxSize(fontDescription, zoomedSize, zoomFactor);
    case RadioPart:
      return radioSize(fontDescription, zoomedSize, zoomFactor);
    case PushButtonPart:
      // Height is reset to auto so that specified heights can be ignored.
      return sizeFromFont(fontDescription,
                          LengthSize(zoomedSize.width(), Length()), zoomFactor,
                          buttonSizes());
    case InnerSpinButtonPart:
      if (!zoomedSize.width().isIntrinsicOrAuto() &&
          !zoomedSize.height().isIntrinsicOrAuto())
        return zoomedSize;
      return sizeFromNSControlSize(stepperControlSizeForFont(fontDescription),
                                   zoomedSize, zoomFactor, stepperSizes());
    default:
      return zoomedSize;
  }
}

LengthSize ThemeMac::minimumControlSize(ControlPart part,
                                        const FontDescription& fontDescription,
                                        float zoomFactor) const {
  switch (part) {
    case SquareButtonPart:
    case ButtonPart:
      return LengthSize(Length(0, Fixed),
                        Length(static_cast<int>(15 * zoomFactor), Fixed));
    case InnerSpinButtonPart: {
      IntSize base = stepperSizes()[NSMiniControlSize];
      return LengthSize(
          Length(static_cast<int>(base.width() * zoomFactor), Fixed),
          Length(static_cast<int>(base.height() * zoomFactor), Fixed));
    }
    default:
      return Theme::minimumControlSize(part, fontDescription, zoomFactor);
  }
}

LengthBox ThemeMac::controlBorder(ControlPart part,
                                  const FontDescription& fontDescription,
                                  const LengthBox& zoomedBox,
                                  float zoomFactor) const {
  switch (part) {
    case SquareButtonPart:
    case ButtonPart:
      return LengthBox(0, zoomedBox.right().value(), 0,
                       zoomedBox.left().value());
    default:
      return Theme::controlBorder(part, fontDescription, zoomedBox, zoomFactor);
  }
}

LengthBox ThemeMac::controlPadding(ControlPart part,
                                   const FontDescription& fontDescription,
                                   const LengthBox& zoomedBox,
                                   float zoomFactor) const {
  switch (part) {
    case PushButtonPart: {
      // Just use 8px.  AppKit wants to use 11px for mini buttons, but that
      // padding is just too large for real-world Web sites (creating a huge
      // necessary minimum width for buttons whose space is by definition
      // constrained, since we select mini only for small cramped environments.
      // This also guarantees the HTML <button> will match our rendering by
      // default, since we're using a consistent padding.
      const int padding = 8 * zoomFactor;
      return LengthBox(2, padding, 3, padding);
    }
    default:
      return Theme::controlPadding(part, fontDescription, zoomedBox,
                                   zoomFactor);
  }
}

void ThemeMac::addVisualOverflow(ControlPart part,
                                 ControlStates states,
                                 float zoomFactor,
                                 IntRect& zoomedRect) const {
  BEGIN_BLOCK_OBJC_EXCEPTIONS
  switch (part) {
    case CheckboxPart: {
      // We inflate the rect as needed to account for padding included in the
      // cell to accommodate the checkbox shadow" and the check.  We don't
      // consider this part of the bounds of the control in WebKit.
      NSCell* cell = checkbox(states, zoomedRect, zoomFactor);
      NSControlSize controlSize = [cell controlSize];
      IntSize zoomedSize = checkboxSizes()[controlSize];
      zoomedSize.setHeight(zoomedSize.height() * zoomFactor);
      zoomedSize.setWidth(zoomedSize.width() * zoomFactor);
      zoomedRect = inflateRect(zoomedRect, zoomedSize,
                               checkboxMargins(controlSize), zoomFactor);
      break;
    }
    case RadioPart: {
      // We inflate the rect as needed to account for padding included in the
      // cell to accommodate the radio button shadow".  We don't consider this
      // part of the bounds of the control in WebKit.
      NSCell* cell = radio(states, zoomedRect, zoomFactor);
      NSControlSize controlSize = [cell controlSize];
      IntSize zoomedSize = radioSizes()[controlSize];
      zoomedSize.setHeight(zoomedSize.height() * zoomFactor);
      zoomedSize.setWidth(zoomedSize.width() * zoomFactor);
      zoomedRect = inflateRect(zoomedRect, zoomedSize,
                               radioMargins(controlSize), zoomFactor);
      break;
    }
    case PushButtonPart:
    case ButtonPart: {
      NSButtonCell* cell = button(part, states, zoomedRect, zoomFactor);
      NSControlSize controlSize = [cell controlSize];

      // We inflate the rect as needed to account for the Aqua button's shadow.
      if ([cell bezelStyle] == NSRoundedBezelStyle) {
        IntSize zoomedSize = buttonSizes()[controlSize];
        zoomedSize.setHeight(zoomedSize.height() * zoomFactor);
        // Buttons don't ever constrain width, so the zoomed width can just be
        // honored.
        zoomedSize.setWidth(zoomedRect.width());
        zoomedRect = inflateRect(zoomedRect, zoomedSize,
                                 buttonMargins(controlSize), zoomFactor);
      }
      break;
    }
    case InnerSpinButtonPart: {
      static const int stepperMargin[4] = {0, 0, 0, 0};
      ControlSize controlSize = controlSizeFromPixelSize(
          stepperSizes(), zoomedRect.size(), zoomFactor);
      IntSize zoomedSize = stepperSizes()[controlSize];
      zoomedSize.setHeight(zoomedSize.height() * zoomFactor);
      zoomedSize.setWidth(zoomedSize.width() * zoomFactor);
      zoomedRect =
          inflateRect(zoomedRect, zoomedSize, stepperMargin, zoomFactor);
      break;
    }
    default:
      break;
  }
  END_BLOCK_OBJC_EXCEPTIONS
}
}
