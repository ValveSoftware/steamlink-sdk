// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2006-2009 Google Inc.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "content/browser/renderer_host/input/web_input_event_builders_mac.h"

#import <ApplicationServices/ApplicationServices.h>
#import <Cocoa/Cocoa.h>

#include <stdint.h>

#include "base/mac/sdk_forward_declarations.h"
#include "base/strings/string_util.h"
#include "content/browser/renderer_host/input/web_input_event_util.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/base/cocoa/cocoa_base_utils.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/keycodes/keyboard_code_conversion_mac.h"

namespace content {

namespace {

// Return true if the target modifier key is up. OS X has an "official" flag
// to test whether either left or right versions of a modifier key are held,
// and "unofficial" flags for the left and right versions independently. This
// function verifies that |target_key_mask| and |otherKeyMask| (which should be
// the left and right versions of a modifier) are consistent with with the
// state of |eitherKeyMask| (which should be the corresponding ""official"
// flag). If they are consistent, it tests |target_key_mask|; otherwise it tests
// |either_key_mask|.
inline bool IsModifierKeyUp(unsigned int flags,
                            unsigned int target_key_mask,
                            unsigned int other_key_mask,
                            unsigned int either_key_mask) {
  bool either_key_down = (flags & either_key_mask) != 0;
  bool target_key_down = (flags & target_key_mask) != 0;
  bool other_key_down = (flags & other_key_mask) != 0;
  if (either_key_down != (target_key_down || other_key_down))
    return !either_key_down;
  return !target_key_down;
}

bool IsKeyUpEvent(NSEvent* event) {
  if ([event type] != NSFlagsChanged)
    return [event type] == NSKeyUp;

  // Unofficial bit-masks for left- and right-hand versions of modifier keys.
  // These values were determined empirically.
  const unsigned int kLeftControlKeyMask = 1 << 0;
  const unsigned int kLeftShiftKeyMask = 1 << 1;
  const unsigned int kRightShiftKeyMask = 1 << 2;
  const unsigned int kLeftCommandKeyMask = 1 << 3;
  const unsigned int kRightCommandKeyMask = 1 << 4;
  const unsigned int kLeftAlternateKeyMask = 1 << 5;
  const unsigned int kRightAlternateKeyMask = 1 << 6;
  const unsigned int kRightControlKeyMask = 1 << 13;

  switch ([event keyCode]) {
    case 54:  // Right Command
      return IsModifierKeyUp([event modifierFlags], kRightCommandKeyMask,
                             kLeftCommandKeyMask, NSCommandKeyMask);
    case 55:  // Left Command
      return IsModifierKeyUp([event modifierFlags], kLeftCommandKeyMask,
                             kRightCommandKeyMask, NSCommandKeyMask);

    case 57:  // Capslock
      return ([event modifierFlags] & NSAlphaShiftKeyMask) == 0;

    case 56:  // Left Shift
      return IsModifierKeyUp([event modifierFlags], kLeftShiftKeyMask,
                             kRightShiftKeyMask, NSShiftKeyMask);
    case 60:  // Right Shift
      return IsModifierKeyUp([event modifierFlags], kRightShiftKeyMask,
                             kLeftShiftKeyMask, NSShiftKeyMask);

    case 58:  // Left Alt
      return IsModifierKeyUp([event modifierFlags], kLeftAlternateKeyMask,
                             kRightAlternateKeyMask, NSAlternateKeyMask);
    case 61:  // Right Alt
      return IsModifierKeyUp([event modifierFlags], kRightAlternateKeyMask,
                             kLeftAlternateKeyMask, NSAlternateKeyMask);

    case 59:  // Left Ctrl
      return IsModifierKeyUp([event modifierFlags], kLeftControlKeyMask,
                             kRightControlKeyMask, NSControlKeyMask);
    case 62:  // Right Ctrl
      return IsModifierKeyUp([event modifierFlags], kRightControlKeyMask,
                             kLeftControlKeyMask, NSControlKeyMask);

    case 63:  // Function
      return ([event modifierFlags] & NSFunctionKeyMask) == 0;
  }
  return false;
}

inline NSString* FilterSpecialCharacter(NSString* str) {
  if ([str length] != 1)
    return str;
  unichar c = [str characterAtIndex:0];
  NSString* result = str;
  if (c == 0x7F) {
    // Backspace should be 8
    result = @"\x8";
  } else if (c >= 0xF700 && c <= 0xF7FF) {
    // Mac private use characters should be @"\0" (@"" won't work)
    // NSDeleteFunctionKey will also go into here
    // Use the range 0xF700~0xF7FF to match
    // http://www.opensource.apple.com/source/WebCore/WebCore-7601.1.55/platform/mac/KeyEventMac.mm
    result = @"\0";
  }
  return result;
}

inline NSString* TextFromEvent(NSEvent* event) {
  if ([event type] == NSFlagsChanged)
    return @"";
  return FilterSpecialCharacter([event characters]);
}

inline NSString* UnmodifiedTextFromEvent(NSEvent* event) {
  if ([event type] == NSFlagsChanged)
    return @"";
  return FilterSpecialCharacter([event charactersIgnoringModifiers]);
}

NSString* KeyIdentifierForKeyEvent(NSEvent* event) {
  if ([event type] == NSFlagsChanged) {
    switch ([event keyCode]) {
      case 54:  // Right Command
      case 55:  // Left Command
        return @"Meta";

      case 57:  // Capslock
        return @"CapsLock";

      case 56:  // Left Shift
      case 60:  // Right Shift
        return @"Shift";

      case 58:  // Left Alt
      case 61:  // Right Alt
        return @"Alt";

      case 59:  // Left Ctrl
      case 62:  // Right Ctrl
        return @"Control";

      // Begin non-Apple addition/modification
      // --------------------------------------
      case 63:  // Function
        return @"Function";

      default:  // Unknown, but this may be a strange/new keyboard.
        return @"Unidentified";
        // End non-Apple addition/modification
        // ----------------------------------------
    }
  }

  NSString* s = [event charactersIgnoringModifiers];
  if ([s length] != 1)
    return @"Unidentified";

  unichar c = [s characterAtIndex:0];
  switch (c) {
    // Each identifier listed in the DOM spec is listed here.
    // Many are simply commented out since they do not appear on standard
    // Macintosh keyboards
    // or are on a key that doesn't have a corresponding character.

    // "Accept"
    // "AllCandidates"

    // "Alt"
    case NSMenuFunctionKey:
      return @"Alt";

    // "Apps"
    // "BrowserBack"
    // "BrowserForward"
    // "BrowserHome"
    // "BrowserRefresh"
    // "BrowserSearch"
    // "BrowserStop"
    // "CapsLock"

    // "Clear"
    case NSClearLineFunctionKey:
      return @"Clear";

    // "CodeInput"
    // "Compose"
    // "Control"
    // "Crsel"
    // "Convert"
    // "Copy"
    // "Cut"

    // "Down"
    case NSDownArrowFunctionKey:
      return @"Down";
    // "End"
    case NSEndFunctionKey:
      return @"End";
    // "Enter"
    case 0x3:
    case 0xA:
    case 0xD:  // Macintosh calls the one on the main keyboard Return, but
               // Windows calls it Enter, so we'll do the same for the DOM
      return @"Enter";

    // "EraseEof"

    // "Execute"
    case NSExecuteFunctionKey:
      return @"Execute";

    // "Exsel"

    // "F1"
    case NSF1FunctionKey:
      return @"F1";
    // "F2"
    case NSF2FunctionKey:
      return @"F2";
    // "F3"
    case NSF3FunctionKey:
      return @"F3";
    // "F4"
    case NSF4FunctionKey:
      return @"F4";
    // "F5"
    case NSF5FunctionKey:
      return @"F5";
    // "F6"
    case NSF6FunctionKey:
      return @"F6";
    // "F7"
    case NSF7FunctionKey:
      return @"F7";
    // "F8"
    case NSF8FunctionKey:
      return @"F8";
    // "F9"
    case NSF9FunctionKey:
      return @"F9";
    // "F10"
    case NSF10FunctionKey:
      return @"F10";
    // "F11"
    case NSF11FunctionKey:
      return @"F11";
    // "F12"
    case NSF12FunctionKey:
      return @"F12";
    // "F13"
    case NSF13FunctionKey:
      return @"F13";
    // "F14"
    case NSF14FunctionKey:
      return @"F14";
    // "F15"
    case NSF15FunctionKey:
      return @"F15";
    // "F16"
    case NSF16FunctionKey:
      return @"F16";
    // "F17"
    case NSF17FunctionKey:
      return @"F17";
    // "F18"
    case NSF18FunctionKey:
      return @"F18";
    // "F19"
    case NSF19FunctionKey:
      return @"F19";
    // "F20"
    case NSF20FunctionKey:
      return @"F20";
    // "F21"
    case NSF21FunctionKey:
      return @"F21";
    // "F22"
    case NSF22FunctionKey:
      return @"F22";
    // "F23"
    case NSF23FunctionKey:
      return @"F23";
    // "F24"
    case NSF24FunctionKey:
      return @"F24";

    // "FinalMode"

    // "Find"
    case NSFindFunctionKey:
      return @"Find";

    // "FullWidth"
    // "HalfWidth"
    // "HangulMode"
    // "HanjaMode"

    // "Help"
    case NSHelpFunctionKey:
      return @"Help";

    // "Hiragana"

    // "Home"
    case NSHomeFunctionKey:
      return @"Home";
    // "Insert"
    case NSInsertFunctionKey:
      return @"Insert";

    // "JapaneseHiragana"
    // "JapaneseKatakana"
    // "JapaneseRomaji"
    // "JunjaMode"
    // "KanaMode"
    // "KanjiMode"
    // "Katakana"
    // "LaunchApplication1"
    // "LaunchApplication2"
    // "LaunchMail"

    // "Left"
    case NSLeftArrowFunctionKey:
      return @"Left";

    // "Meta"
    // "MediaNextTrack"
    // "MediaPlayPause"
    // "MediaPreviousTrack"
    // "MediaStop"

    // "ModeChange"
    case NSModeSwitchFunctionKey:
      return @"ModeChange";

    // "Nonconvert"
    // "NumLock"

    // "PageDown"
    case NSPageDownFunctionKey:
      return @"PageDown";
    // "PageUp"
    case NSPageUpFunctionKey:
      return @"PageUp";

    // "Paste"

    // "Pause"
    case NSPauseFunctionKey:
      return @"Pause";

    // "Play"
    // "PreviousCandidate"

    // "PrintScreen"
    case NSPrintScreenFunctionKey:
      return @"PrintScreen";

    // "Process"
    // "Props"

    // "Right"
    case NSRightArrowFunctionKey:
      return @"Right";

    // "RomanCharacters"

    // "Scroll"
    case NSScrollLockFunctionKey:
      return @"Scroll";
    // "Select"
    case NSSelectFunctionKey:
      return @"Select";

    // "SelectMedia"
    // "Shift"

    // "Stop"
    case NSStopFunctionKey:
      return @"Stop";
    // "Up"
    case NSUpArrowFunctionKey:
      return @"Up";
    // "Undo"
    case NSUndoFunctionKey:
      return @"Undo";

    // "VolumeDown"
    // "VolumeMute"
    // "VolumeUp"
    // "Win"
    // "Zoom"

    // More function keys, not in the key identifier specification.
    case NSF25FunctionKey:
      return @"F25";
    case NSF26FunctionKey:
      return @"F26";
    case NSF27FunctionKey:
      return @"F27";
    case NSF28FunctionKey:
      return @"F28";
    case NSF29FunctionKey:
      return @"F29";
    case NSF30FunctionKey:
      return @"F30";
    case NSF31FunctionKey:
      return @"F31";
    case NSF32FunctionKey:
      return @"F32";
    case NSF33FunctionKey:
      return @"F33";
    case NSF34FunctionKey:
      return @"F34";
    case NSF35FunctionKey:
      return @"F35";

    // Turn 0x7F into 0x08, because backspace needs to always be 0x08.
    case 0x7F:
      return @"U+0008";
    // Standard says that DEL becomes U+007F.
    case NSDeleteFunctionKey:
      return @"U+007F";

    // Always use 0x09 for tab instead of AppKit's backtab character.
    case NSBackTabCharacter:
      return @"U+0009";

    case NSBeginFunctionKey:
    case NSBreakFunctionKey:
    case NSClearDisplayFunctionKey:
    case NSDeleteCharFunctionKey:
    case NSDeleteLineFunctionKey:
    case NSInsertCharFunctionKey:
    case NSInsertLineFunctionKey:
    case NSNextFunctionKey:
    case NSPrevFunctionKey:
    case NSPrintFunctionKey:
    case NSRedoFunctionKey:
    case NSResetFunctionKey:
    case NSSysReqFunctionKey:
    case NSSystemFunctionKey:
    case NSUserFunctionKey:
    // FIXME: We should use something other than the vendor-area Unicode values
    // for the above keys.
    // For now, just fall through to the default.
    default:
      return [NSString stringWithFormat:@"U+%04X", base::ToUpperASCII(c)];
  }
}

// End Apple code.
// ----------------------------------------------------------------------------

int ModifiersFromEvent(NSEvent* event) {
  int modifiers = 0;

  if ([event modifierFlags] & NSControlKeyMask)
    modifiers |= blink::WebInputEvent::ControlKey;
  if ([event modifierFlags] & NSShiftKeyMask)
    modifiers |= blink::WebInputEvent::ShiftKey;
  if ([event modifierFlags] & NSAlternateKeyMask)
    modifiers |= blink::WebInputEvent::AltKey;
  if ([event modifierFlags] & NSCommandKeyMask)
    modifiers |= blink::WebInputEvent::MetaKey;
  if ([event modifierFlags] & NSAlphaShiftKeyMask)
    modifiers |= blink::WebInputEvent::CapsLockOn;

  // The return value of 1 << 0 corresponds to the left mouse button,
  // 1 << 1 corresponds to the right mouse button,
  // 1 << n, n >= 2 correspond to other mouse buttons.
  NSUInteger pressed_buttons = [NSEvent pressedMouseButtons];

  if (pressed_buttons & (1 << 0))
    modifiers |= blink::WebInputEvent::LeftButtonDown;
  if (pressed_buttons & (1 << 1))
    modifiers |= blink::WebInputEvent::RightButtonDown;
  if (pressed_buttons & (1 << 2))
    modifiers |= blink::WebInputEvent::MiddleButtonDown;

  return modifiers;
}

void SetWebEventLocationFromEventInView(blink::WebMouseEvent* result,
                                        NSEvent* event,
                                        NSView* view) {
  NSPoint screen_local = ui::ConvertPointFromWindowToScreen(
      [view window], [event locationInWindow]);
  result->globalX = screen_local.x;
  // Flip y.
  NSScreen* primary_screen = ([[NSScreen screens] count] > 0)
                                 ? [[NSScreen screens] firstObject]
                                 : nil;
  if (primary_screen)
    result->globalY = [primary_screen frame].size.height - screen_local.y;
  else
    result->globalY = screen_local.y;

  NSPoint content_local =
      [view convertPoint:[event locationInWindow] fromView:nil];
  result->x = content_local.x;
  result->y = [view frame].size.height - content_local.y;  // Flip y.

  result->windowX = result->x;
  result->windowY = result->y;

  result->movementX = [event deltaX];
  result->movementY = [event deltaY];
}

bool IsSystemKeyEvent(const blink::WebKeyboardEvent& event) {
  // Windows and Linux set |isSystemKey| if alt is down. Blink looks at this
  // flag to decide if it should handle a key or not. E.g. alt-left/right
  // shouldn't be used by Blink to scroll the current page, because we want
  // to get that key back for it to do history navigation. Hence, the
  // corresponding situation on OS X is to set this for cmd key presses.

  // cmd-b and and cmd-i are system wide key bindings that OS X doesn't
  // handle for us, so the editor handles them.
  int modifiers = event.modifiers & blink::WebInputEvent::InputModifiers;
  if (modifiers == blink::WebInputEvent::MetaKey &&
      event.windowsKeyCode == ui::VKEY_B)
    return false;
  if (modifiers == blink::WebInputEvent::MetaKey &&
      event.windowsKeyCode == ui::VKEY_I)
    return false;

  return event.modifiers & blink::WebInputEvent::MetaKey;
}

blink::WebMouseWheelEvent::Phase PhaseForNSEventPhase(
    NSEventPhase event_phase) {
  uint32_t phase = blink::WebMouseWheelEvent::PhaseNone;
  if (event_phase & NSEventPhaseBegan)
    phase |= blink::WebMouseWheelEvent::PhaseBegan;
  if (event_phase & NSEventPhaseStationary)
    phase |= blink::WebMouseWheelEvent::PhaseStationary;
  if (event_phase & NSEventPhaseChanged)
    phase |= blink::WebMouseWheelEvent::PhaseChanged;
  if (event_phase & NSEventPhaseEnded)
    phase |= blink::WebMouseWheelEvent::PhaseEnded;
  if (event_phase & NSEventPhaseCancelled)
    phase |= blink::WebMouseWheelEvent::PhaseCancelled;
  if (event_phase & NSEventPhaseMayBegin)
    phase |= blink::WebMouseWheelEvent::PhaseMayBegin;
  return static_cast<blink::WebMouseWheelEvent::Phase>(phase);
}

blink::WebMouseWheelEvent::Phase PhaseForEvent(NSEvent* event) {
  if (![event respondsToSelector:@selector(phase)])
    return blink::WebMouseWheelEvent::PhaseNone;

  NSEventPhase event_phase = [event phase];
  return PhaseForNSEventPhase(event_phase);
}

blink::WebMouseWheelEvent::Phase MomentumPhaseForEvent(NSEvent* event) {
  if (![event respondsToSelector:@selector(momentumPhase)])
    return blink::WebMouseWheelEvent::PhaseNone;

  NSEventPhase event_momentum_phase = [event momentumPhase];
  return PhaseForNSEventPhase(event_momentum_phase);
}

ui::DomKey DomKeyFromEvent(NSEvent* event) {
  ui::DomKey key = ui::DomKeyFromNSEvent(event);
  if (key != ui::DomKey::NONE)
    return key;
  return ui::DomKey::UNIDENTIFIED;
}

}  // namespace

blink::WebKeyboardEvent WebKeyboardEventBuilder::Build(NSEvent* event) {
  blink::WebKeyboardEvent result;

  result.type = IsKeyUpEvent(event) ? blink::WebInputEvent::KeyUp
                                    : blink::WebInputEvent::RawKeyDown;

  result.modifiers = ModifiersFromEvent(event);

  if (([event type] != NSFlagsChanged) && [event isARepeat])
    result.modifiers |= blink::WebInputEvent::IsAutoRepeat;

  ui::DomCode dom_code = ui::DomCodeFromNSEvent(event);
  result.windowsKeyCode =
      ui::LocatedToNonLocatedKeyboardCode(ui::KeyboardCodeFromNSEvent(event));
  result.modifiers |= DomCodeToWebInputEventModifiers(dom_code);
  result.nativeKeyCode = [event keyCode];
  result.domCode = static_cast<int>(dom_code);
  result.domKey = DomKeyFromEvent(event);
  NSString* text_str = TextFromEvent(event);
  NSString* unmodified_str = UnmodifiedTextFromEvent(event);
  NSString* identifier_str = KeyIdentifierForKeyEvent(event);

  // Begin Apple code, copied from KeyEventMac.mm

  // Always use 13 for Enter/Return -- we don't want to use AppKit's
  // different character for Enter.
  if (result.windowsKeyCode == '\r') {
    text_str = @"\r";
    unmodified_str = @"\r";
  }

  // Always use 9 for tab -- we don't want to use AppKit's different character
  // for shift-tab.
  if (result.windowsKeyCode == 9) {
    text_str = @"\x9";
    unmodified_str = @"\x9";
  }

  // End Apple code.

  if ([text_str length] < blink::WebKeyboardEvent::textLengthCap &&
      [unmodified_str length] < blink::WebKeyboardEvent::textLengthCap) {
    [text_str getCharacters:&result.text[0]];
    [unmodified_str getCharacters:&result.unmodifiedText[0]];
  } else
    NOTIMPLEMENTED();

  [identifier_str getCString:&result.keyIdentifier[0]
                   maxLength:sizeof(result.keyIdentifier)
                    encoding:NSASCIIStringEncoding];

  result.timeStampSeconds = [event timestamp];
  result.isSystemKey = IsSystemKeyEvent(result);

  return result;
}

// WebMouseEvent --------------------------------------------------------------

blink::WebMouseEvent WebMouseEventBuilder::Build(NSEvent* event, NSView* view) {
  blink::WebMouseEvent result;

  result.clickCount = 0;

  NSEventType type = [event type];
  switch (type) {
    case NSMouseExited:
      result.type = blink::WebInputEvent::MouseLeave;
      result.button = blink::WebMouseEvent::ButtonNone;
      break;
    case NSLeftMouseDown:
      result.type = blink::WebInputEvent::MouseDown;
      result.clickCount = [event clickCount];
      result.button = blink::WebMouseEvent::ButtonLeft;
      break;
    case NSOtherMouseDown:
      result.type = blink::WebInputEvent::MouseDown;
      result.clickCount = [event clickCount];
      result.button = blink::WebMouseEvent::ButtonMiddle;
      break;
    case NSRightMouseDown:
      result.type = blink::WebInputEvent::MouseDown;
      result.clickCount = [event clickCount];
      result.button = blink::WebMouseEvent::ButtonRight;
      break;
    case NSLeftMouseUp:
      result.type = blink::WebInputEvent::MouseUp;
      result.clickCount = [event clickCount];
      result.button = blink::WebMouseEvent::ButtonLeft;
      break;
    case NSOtherMouseUp:
      result.type = blink::WebInputEvent::MouseUp;
      result.clickCount = [event clickCount];
      result.button = blink::WebMouseEvent::ButtonMiddle;
      break;
    case NSRightMouseUp:
      result.type = blink::WebInputEvent::MouseUp;
      result.clickCount = [event clickCount];
      result.button = blink::WebMouseEvent::ButtonRight;
      break;
    case NSMouseMoved:
    case NSMouseEntered:
      result.type = blink::WebInputEvent::MouseMove;
      break;
    case NSLeftMouseDragged:
      result.type = blink::WebInputEvent::MouseMove;
      result.button = blink::WebMouseEvent::ButtonLeft;
      break;
    case NSOtherMouseDragged:
      result.type = blink::WebInputEvent::MouseMove;
      result.button = blink::WebMouseEvent::ButtonMiddle;
      break;
    case NSRightMouseDragged:
      result.type = blink::WebInputEvent::MouseMove;
      result.button = blink::WebMouseEvent::ButtonRight;
      break;
    default:
      NOTIMPLEMENTED();
  }

  SetWebEventLocationFromEventInView(&result, event, view);

  result.modifiers = ModifiersFromEvent(event);

  result.timeStampSeconds = [event timestamp];

  // For NSMouseExited and NSMouseEntered, they do not have a subtype. Styluses
  // and mouses share the same cursor, so we will set their pointerType as
  // Unknown for now.
  if (type == NSMouseExited || type == NSMouseEntered) {
    result.pointerType = blink::WebPointerProperties::PointerType::Unknown;
    return result;
  }

  // For other mouse events and touchpad events, the pointer type is mouse.
  // For all other tablet events, the pointer type will be just pen.
  NSEventSubtype subtype = [event subtype];
  if (subtype == NSTabletPointEventSubtype ||
      subtype == NSTabletProximityEventSubtype) {
    result.pointerType = blink::WebPointerProperties::PointerType::Pen;
  } else {
    result.pointerType = blink::WebPointerProperties::PointerType::Mouse;
  }
  result.id = [event deviceID];
  result.force = [event pressure];
  return result;
}

// WebMouseWheelEvent ---------------------------------------------------------

blink::WebMouseWheelEvent WebMouseWheelEventBuilder::Build(
    NSEvent* event,
    NSView* view,
    bool can_rubberband_left,
    bool can_rubberband_right) {
  blink::WebMouseWheelEvent result;

  result.type = blink::WebInputEvent::MouseWheel;
  result.button = blink::WebMouseEvent::ButtonNone;

  result.modifiers = ModifiersFromEvent(event);

  SetWebEventLocationFromEventInView(&result, event, view);

  result.canRubberbandLeft = can_rubberband_left;
  result.canRubberbandRight = can_rubberband_right;

  // Of Mice and Men
  // ---------------
  //
  // There are three types of scroll data available on a scroll wheel CGEvent.
  // Apple's documentation ([1]) is rather vague in their differences, and not
  // terribly helpful in deciding which to use. This is what's really going on.
  //
  // First, these events behave very differently depending on whether a standard
  // wheel mouse is used (one that scrolls in discrete units) or a
  // trackpad/Mighty Mouse is used (which both provide continuous scrolling).
  // You must check to see which was used for the event by testing the
  // kCGScrollWheelEventIsContinuous field.
  //
  // Second, these events refer to "axes". Axis 1 is the y-axis, and axis 2 is
  // the x-axis.
  //
  // Third, there is a concept of mouse acceleration. Scrolling the same amount
  // of physical distance will give you different results logically depending on
  // whether you scrolled a little at a time or in one continuous motion. Some
  // fields account for this while others do not.
  //
  // Fourth, for trackpads there is a concept of chunkiness. When scrolling
  // continuously, events can be delivered in chunks. That is to say, lots of
  // scroll events with delta 0 will be delivered, and every so often an event
  // with a non-zero delta will be delivered, containing the accumulated deltas
  // from all the intermediate moves. [2]
  //
  // For notchy wheel mice (kCGScrollWheelEventIsContinuous == 0)
  // ------------------------------------------------------------
  //
  // kCGScrollWheelEventDeltaAxis*
  //   This is the rawest of raw events. For each mouse notch you get a value of
  //   +1/-1. This does not take acceleration into account and thus is less
  //   useful for building UIs.
  //
  // kCGScrollWheelEventPointDeltaAxis*
  //   This is smarter. In general, for each mouse notch you get a value of
  //   +1/-1, but this _does_ take acceleration into account, so you will get
  //   larger values on longer scrolls. This field would be ideal for building
  //   UIs except for one nasty bug: when the shift key is pressed, this set of
  //   fields fails to move the value into the axis2 field (the other two types
  //   of data do). This wouldn't be so bad except for the fact that while the
  //   number of axes is used in the creation of a CGScrollWheelEvent, there is
  //   no way to get that information out of the event once created.
  //
  // kCGScrollWheelEventFixedPtDeltaAxis*
  //   This is a fixed value, and for each mouse notch you get a value of
  //   +0.1/-0.1 (but, like above, scaled appropriately for acceleration). This
  //   value takes acceleration into account, and in fact is identical to the
  //   results you get from -[NSEvent delta*]. (That is, if you linked on Tiger
  //   or greater; see [2] for details.)
  //
  // A note about continuous devices
  // -------------------------------
  //
  // There are two devices that provide continuous scrolling events (trackpads
  // and Mighty Mouses) and they behave rather differently. The Mighty Mouse
  // behaves a lot like a regular mouse. There is no chunking, and the
  // FixedPtDelta values are the PointDelta values multiplied by 0.1. With the
  // trackpad, though, there is chunking. While the FixedPtDelta values are
  // reasonable (they occur about every fifth event but have values five times
  // larger than usual) the Delta values are unreasonable. They don't appear to
  // accumulate properly.
  //
  // For continuous devices (kCGScrollWheelEventIsContinuous != 0)
  // -------------------------------------------------------------
  //
  // kCGScrollWheelEventDeltaAxis*
  //   This provides values with no acceleration. With a trackpad, these values
  //   are chunked but each non-zero value does not appear to be cumulative.
  //   This seems to be a bug.
  //
  // kCGScrollWheelEventPointDeltaAxis*
  //   This provides values with acceleration. With a trackpad, these values are
  //   not chunked and are highly accurate.
  //
  // kCGScrollWheelEventFixedPtDeltaAxis*
  //   This provides values with acceleration. With a trackpad, these values are
  //   chunked but unlike Delta events are properly cumulative.
  //
  // Summary
  // -------
  //
  // In general the best approach to take is: determine if the event is
  // continuous. If it is not, then use the FixedPtDelta events (or just stick
  // with Cocoa events). They provide both acceleration and proper horizontal
  // scrolling. If the event is continuous, then doing pixel scrolling with the
  // PointDelta is the way to go. In general, avoid the Delta events. They're
  // the oldest (dating back to 10.4, before CGEvents were public) but they lack
  // acceleration and precision, making them useful only in specific edge cases.
  //
  // References
  // ----------
  //
  // [1]
  // <http://developer.apple.com/documentation/Carbon/Reference/QuartzEventServicesRef/Reference/reference.html>
  // [2] <http://developer.apple.com/releasenotes/Cocoa/AppKitOlderNotes.html>
  //     Scroll to the section headed "NSScrollWheel events".
  //
  // P.S. The "smooth scrolling" option in the system preferences is utterly
  // unrelated to any of this.

  CGEventRef cg_event = [event CGEvent];
  DCHECK(cg_event);

  // Wheel ticks are supposed to be raw, unaccelerated values, one per physical
  // mouse wheel notch. The delta event is perfect for this (being a good
  // "specific edge case" as mentioned above). Trackpads, unfortunately, do
  // event chunking, and sending mousewheel events with 0 ticks causes some
  // websites to malfunction. Therefore, for all continuous input devices we use
  // the point delta data instead, since we cannot distinguish trackpad data
  // from data from any other continuous device.

  // Conversion between wheel delta amounts and number of pixels to scroll.
  static const double kScrollbarPixelsPerCocoaTick = 40.0;

  if (CGEventGetIntegerValueField(cg_event, kCGScrollWheelEventIsContinuous)) {
    result.deltaX = CGEventGetIntegerValueField(
        cg_event, kCGScrollWheelEventPointDeltaAxis2);
    result.deltaY = CGEventGetIntegerValueField(
        cg_event, kCGScrollWheelEventPointDeltaAxis1);
    result.wheelTicksX = result.deltaX / kScrollbarPixelsPerCocoaTick;
    result.wheelTicksY = result.deltaY / kScrollbarPixelsPerCocoaTick;
    result.hasPreciseScrollingDeltas = true;
  } else {
    result.deltaX = [event deltaX] * kScrollbarPixelsPerCocoaTick;
    result.deltaY = [event deltaY] * kScrollbarPixelsPerCocoaTick;
    result.wheelTicksY =
        CGEventGetIntegerValueField(cg_event, kCGScrollWheelEventDeltaAxis1);
    result.wheelTicksX =
        CGEventGetIntegerValueField(cg_event, kCGScrollWheelEventDeltaAxis2);
  }

  result.timeStampSeconds = [event timestamp];

  result.phase = PhaseForEvent(event);
  result.momentumPhase = MomentumPhaseForEvent(event);

  return result;
}

blink::WebGestureEvent WebGestureEventBuilder::Build(NSEvent* event,
                                                     NSView* view) {
  blink::WebGestureEvent result;

  // Use a temporary WebMouseEvent to get the location.
  blink::WebMouseEvent temp;

  SetWebEventLocationFromEventInView(&temp, event, view);
  result.x = temp.x;
  result.y = temp.y;
  result.globalX = temp.globalX;
  result.globalY = temp.globalY;

  result.modifiers = ModifiersFromEvent(event);
  result.timeStampSeconds = [event timestamp];

  result.sourceDevice = blink::WebGestureDeviceTouchpad;
  switch ([event type]) {
    case NSEventTypeMagnify:
      result.type = blink::WebInputEvent::GesturePinchUpdate;
      result.data.pinchUpdate.scale = [event magnification] + 1.0;
      break;
    case NSEventTypeSmartMagnify:
      // Map the Cocoa "double-tap with two fingers" zoom gesture to regular
      // GestureDoubleTap, because the effect is similar to single-finger
      // double-tap zoom on mobile platforms. Note that tapCount is set to 1
      // because the gesture type already encodes that information.
      result.type = blink::WebInputEvent::GestureDoubleTap;
      result.data.tap.tapCount = 1;
      break;
    case NSEventTypeBeginGesture:
    case NSEventTypeEndGesture:
      // The specific type of a gesture is not defined when the gesture begin
      // and end NSEvents come in. Leave them undefined. The caller will need
      // to specify them when the gesture is differentiated.
      result.type = blink::WebInputEvent::Undefined;
      break;
    default:
      NOTIMPLEMENTED();
      result.type = blink::WebInputEvent::Undefined;
  }

  return result;
}

}  // namespace content
