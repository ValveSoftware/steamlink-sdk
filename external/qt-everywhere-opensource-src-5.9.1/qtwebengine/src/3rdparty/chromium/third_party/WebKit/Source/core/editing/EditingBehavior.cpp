/*
 * Copyright (C) 2006, 2007 Apple, Inc.  All rights reserved.
 * Copyright (C) 2012 Google, Inc.  All rights reserved.
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

#include "core/editing/EditingBehavior.h"

#include "core/events/KeyboardEvent.h"
#include "platform/KeyboardCodes.h"
#include "public/platform/WebInputEvent.h"

namespace blink {

namespace {

//
// The below code was adapted from the WebKit file webview.cpp
//

const unsigned CtrlKey = WebInputEvent::ControlKey;
const unsigned AltKey = WebInputEvent::AltKey;
const unsigned ShiftKey = WebInputEvent::ShiftKey;
const unsigned MetaKey = WebInputEvent::MetaKey;
#if OS(MACOSX)
// Aliases for the generic key defintions to make kbd shortcuts definitions more
// readable on OS X.
const unsigned OptionKey = AltKey;

// Do not use this constant for anything but cursor movement commands. Keys
// with cmd set have their |isSystemKey| bit set, so chances are the shortcut
// will not be executed. Another, less important, reason is that shortcuts
// defined in the layoutObject do not blink the menu item that they triggered.
// See http://crbug.com/25856 and the bugs linked from there for details.
const unsigned CommandKey = MetaKey;
#endif

// Keys with special meaning. These will be delegated to the editor using
// the execCommand() method
struct KeyboardCodeKeyDownEntry {
  unsigned virtualKey;
  unsigned modifiers;
  const char* name;
};

struct KeyboardCodeKeyPressEntry {
  unsigned charCode;
  unsigned modifiers;
  const char* name;
};

// DomKey has a broader range than KeyboardCode, we need DomKey to handle some
// special keys.
// Note: We cannot use DomKey for printable keys since it may vary based on
// locale.
struct DomKeyKeyDownEntry {
  const char* key;
  unsigned modifiers;
  const char* name;
};

// Key bindings with command key on Mac and alt key on other platforms are
// marked as system key events and will be ignored (with the exception
// of Command-B and Command-I) so they shouldn't be added here.
const KeyboardCodeKeyDownEntry kKeyboardCodeKeyDownEntries[] = {
    {VKEY_LEFT, 0, "MoveLeft"},
    {VKEY_LEFT, ShiftKey, "MoveLeftAndModifySelection"},
#if OS(MACOSX)
    {VKEY_LEFT, OptionKey, "MoveWordLeft"},
    {VKEY_LEFT, OptionKey | ShiftKey, "MoveWordLeftAndModifySelection"},
#else
    {VKEY_LEFT, CtrlKey, "MoveWordLeft"},
    {VKEY_LEFT, CtrlKey | ShiftKey, "MoveWordLeftAndModifySelection"},
#endif
    {VKEY_RIGHT, 0, "MoveRight"},
    {VKEY_RIGHT, ShiftKey, "MoveRightAndModifySelection"},
#if OS(MACOSX)
    {VKEY_RIGHT, OptionKey, "MoveWordRight"},
    {VKEY_RIGHT, OptionKey | ShiftKey, "MoveWordRightAndModifySelection"},
#else
    {VKEY_RIGHT, CtrlKey, "MoveWordRight"},
    {VKEY_RIGHT, CtrlKey | ShiftKey, "MoveWordRightAndModifySelection"},
#endif
    {VKEY_UP, 0, "MoveUp"},
    {VKEY_UP, ShiftKey, "MoveUpAndModifySelection"},
    {VKEY_PRIOR, ShiftKey, "MovePageUpAndModifySelection"},
    {VKEY_DOWN, 0, "MoveDown"},
    {VKEY_DOWN, ShiftKey, "MoveDownAndModifySelection"},
    {VKEY_NEXT, ShiftKey, "MovePageDownAndModifySelection"},
#if !OS(MACOSX)
    {VKEY_UP, CtrlKey, "MoveParagraphBackward"},
    {VKEY_UP, CtrlKey | ShiftKey, "MoveParagraphBackwardAndModifySelection"},
    {VKEY_DOWN, CtrlKey, "MoveParagraphForward"},
    {VKEY_DOWN, CtrlKey | ShiftKey, "MoveParagraphForwardAndModifySelection"},
    {VKEY_PRIOR, 0, "MovePageUp"},
    {VKEY_NEXT, 0, "MovePageDown"},
#endif
    {VKEY_HOME, 0, "MoveToBeginningOfLine"},
    {VKEY_HOME, ShiftKey, "MoveToBeginningOfLineAndModifySelection"},
#if OS(MACOSX)
    {VKEY_PRIOR, OptionKey, "MovePageUp"},
    {VKEY_NEXT, OptionKey, "MovePageDown"},
#endif
#if !OS(MACOSX)
    {VKEY_HOME, CtrlKey, "MoveToBeginningOfDocument"},
    {VKEY_HOME, CtrlKey | ShiftKey,
     "MoveToBeginningOfDocumentAndModifySelection"},
#endif
    {VKEY_END, 0, "MoveToEndOfLine"},
    {VKEY_END, ShiftKey, "MoveToEndOfLineAndModifySelection"},
#if !OS(MACOSX)
    {VKEY_END, CtrlKey, "MoveToEndOfDocument"},
    {VKEY_END, CtrlKey | ShiftKey, "MoveToEndOfDocumentAndModifySelection"},
#endif
    {VKEY_BACK, 0, "DeleteBackward"},
    {VKEY_BACK, ShiftKey, "DeleteBackward"},
    {VKEY_DELETE, 0, "DeleteForward"},
#if OS(MACOSX)
    {VKEY_BACK, OptionKey, "DeleteWordBackward"},
    {VKEY_DELETE, OptionKey, "DeleteWordForward"},
#else
    {VKEY_BACK, CtrlKey, "DeleteWordBackward"},
    {VKEY_DELETE, CtrlKey, "DeleteWordForward"},
#endif
#if OS(MACOSX)
    {'B', CommandKey, "ToggleBold"},
    {'I', CommandKey, "ToggleItalic"},
#else
    {'B', CtrlKey, "ToggleBold"},
    {'I', CtrlKey, "ToggleItalic"},
#endif
    {'U', CtrlKey, "ToggleUnderline"},
    {VKEY_ESCAPE, 0, "Cancel"},
    {VKEY_OEM_PERIOD, CtrlKey, "Cancel"},
    {VKEY_TAB, 0, "InsertTab"},
    {VKEY_TAB, ShiftKey, "InsertBacktab"},
    {VKEY_RETURN, 0, "InsertNewline"},
    {VKEY_RETURN, CtrlKey, "InsertNewline"},
    {VKEY_RETURN, AltKey, "InsertNewline"},
    {VKEY_RETURN, AltKey | ShiftKey, "InsertNewline"},
    {VKEY_RETURN, ShiftKey, "InsertLineBreak"},
    {VKEY_INSERT, CtrlKey, "Copy"},
    {VKEY_INSERT, ShiftKey, "Paste"},
    {VKEY_DELETE, ShiftKey, "Cut"},
#if !OS(MACOSX)
    // On OS X, we pipe these back to the browser, so that it can do menu item
    // blinking.
    {'C', CtrlKey, "Copy"},
    {'V', CtrlKey, "Paste"},
    {'V', CtrlKey | ShiftKey, "PasteAndMatchStyle"},
    {'X', CtrlKey, "Cut"},
    {'A', CtrlKey, "SelectAll"},
    {'Z', CtrlKey, "Undo"},
    {'Z', CtrlKey | ShiftKey, "Redo"},
    {'Y', CtrlKey, "Redo"},
#endif
    {VKEY_INSERT, 0, "OverWrite"},
};

const KeyboardCodeKeyPressEntry kKeyboardCodeKeyPressEntries[] = {
    {'\t', 0, "InsertTab"},
    {'\t', ShiftKey, "InsertBacktab"},
    {'\r', 0, "InsertNewline"},
    {'\r', ShiftKey, "InsertLineBreak"},
};

const DomKeyKeyDownEntry kDomKeyKeyDownEntries[] = {
    {"Copy", 0, "Copy"},
    {"Cut", 0, "Cut"},
    {"Paste", 0, "Paste"},
};

const char* lookupCommandNameFromDomKeyKeyDown(const String& key,
                                               unsigned modifiers) {
  // This table is not likely to grow, so sequential search is fine here.
  for (const auto& entry : kDomKeyKeyDownEntries) {
    if (key == entry.key && modifiers == entry.modifiers)
      return entry.name;
  }
  return nullptr;
}

}  // anonymous namespace

const char* EditingBehavior::interpretKeyEvent(
    const KeyboardEvent& event) const {
  const WebKeyboardEvent* keyEvent = event.keyEvent();
  if (!keyEvent)
    return "";

  static HashMap<int, const char*>* keyDownCommandsMap = nullptr;
  static HashMap<int, const char*>* keyPressCommandsMap = nullptr;

  if (!keyDownCommandsMap) {
    keyDownCommandsMap = new HashMap<int, const char*>;
    keyPressCommandsMap = new HashMap<int, const char*>;

    for (const auto& entry : kKeyboardCodeKeyDownEntries) {
      keyDownCommandsMap->set(entry.modifiers << 16 | entry.virtualKey,
                              entry.name);
    }

    for (const auto& entry : kKeyboardCodeKeyPressEntries) {
      keyPressCommandsMap->set(entry.modifiers << 16 | entry.charCode,
                               entry.name);
    }
  }

  unsigned modifiers =
      keyEvent->modifiers & (ShiftKey | AltKey | CtrlKey | MetaKey);

  if (keyEvent->type == WebInputEvent::RawKeyDown) {
    int mapKey = modifiers << 16 | event.keyCode();
    const char* name = mapKey ? keyDownCommandsMap->get(mapKey) : nullptr;
    if (!name)
      name = lookupCommandNameFromDomKeyKeyDown(event.key(), modifiers);
    return name;
  }

  int mapKey = modifiers << 16 | event.charCode();
  return mapKey ? keyPressCommandsMap->get(mapKey) : 0;
}

bool EditingBehavior::shouldInsertCharacter(const KeyboardEvent& event) const {
  if (event.keyEvent()->text[1] != 0)
    return true;

  // On Gtk/Linux, it emits key events with ASCII text and ctrl on for ctrl-<x>.
  // In Webkit, EditorClient::handleKeyboardEvent in
  // WebKit/gtk/WebCoreSupport/EditorClientGtk.cpp drop such events.
  // On Mac, it emits key events with ASCII text and meta on for Command-<x>.
  // These key events should not emit text insert event.
  // Alt key would be used to insert alternative character, so we should let
  // through. Also note that Ctrl-Alt combination equals to AltGr key which is
  // also used to insert alternative character.
  // http://code.google.com/p/chromium/issues/detail?id=10846
  // Windows sets both alt and meta are on when "Alt" key pressed.
  // http://code.google.com/p/chromium/issues/detail?id=2215
  // Also, we should not rely on an assumption that keyboards don't
  // send ASCII characters when pressing a control key on Windows,
  // which may be configured to do it so by user.
  // See also http://en.wikipedia.org/wiki/Keyboard_Layout
  // FIXME(ukai): investigate more detail for various keyboard layout.
  UChar ch = event.keyEvent()->text[0U];

  // Don't insert null or control characters as they can result in
  // unexpected behaviour
  if (ch < ' ')
    return false;
#if OS(LINUX)
  // According to XKB map no keyboard combinations with ctrl key are mapped to
  // printable characters, however we need the filter as the DomKey/text could
  // contain printable characters.
  if (event.ctrlKey())
    return false;
#elif !OS(WIN)
  // Don't insert ASCII character if ctrl w/o alt or meta is on.
  // On Mac, we should ignore events when meta is on (Command-<x>).
  if (ch < 0x80) {
    if (event.ctrlKey() && !event.altKey())
      return false;
#if OS(MACOSX)
    if (event.metaKey())
      return false;
#endif
  }
#endif

  return true;
}
}  // namespace blink
