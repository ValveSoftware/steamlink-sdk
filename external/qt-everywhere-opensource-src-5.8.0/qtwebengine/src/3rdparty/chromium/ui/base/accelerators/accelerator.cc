// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/accelerator.h"

#include <stdint.h>

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/strings/grit/ui_strings.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#if !defined(OS_WIN) && (defined(USE_AURA) || defined(OS_MACOSX))
#include "ui/events/keycodes/keyboard_code_conversion.h"
#endif

namespace ui {

namespace {

const int kEventFlagsMask = ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN |
                            ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN;

}  // namespace

Accelerator::Accelerator()
    : key_code_(ui::VKEY_UNKNOWN),
      type_(ui::ET_KEY_PRESSED),
      modifiers_(0),
      is_repeat_(false) {
}

Accelerator::Accelerator(KeyboardCode keycode, int modifiers)
    : key_code_(keycode),
      type_(ui::ET_KEY_PRESSED),
      modifiers_(modifiers),
      is_repeat_(false) {
}

Accelerator::Accelerator(const KeyEvent& key_event)
    : key_code_(key_event.key_code()),
      type_(key_event.type()),
      modifiers_(MaskOutKeyEventFlags(key_event.flags())),
      is_repeat_(key_event.is_repeat()) {
}

Accelerator::Accelerator(const Accelerator& accelerator) {
  key_code_ = accelerator.key_code_;
  type_ = accelerator.type_;
  modifiers_ = accelerator.modifiers_;
  is_repeat_ = accelerator.is_repeat_;
  if (accelerator.platform_accelerator_.get())
    platform_accelerator_ = accelerator.platform_accelerator_->CreateCopy();
}

Accelerator::~Accelerator() {
}

// static
int Accelerator::MaskOutKeyEventFlags(int flags) {
  return flags & kEventFlagsMask;
}

Accelerator& Accelerator::operator=(const Accelerator& accelerator) {
  if (this != &accelerator) {
    key_code_ = accelerator.key_code_;
    type_ = accelerator.type_;
    modifiers_ = accelerator.modifiers_;
    is_repeat_ = accelerator.is_repeat_;
    if (accelerator.platform_accelerator_.get())
      platform_accelerator_ = accelerator.platform_accelerator_->CreateCopy();
    else
      platform_accelerator_.reset();
  }
  return *this;
}

bool Accelerator::operator <(const Accelerator& rhs) const {
  if (key_code_ != rhs.key_code_)
    return key_code_ < rhs.key_code_;
  if (type_ != rhs.type_)
    return type_ < rhs.type_;
  return modifiers_ < rhs.modifiers_;
}

bool Accelerator::operator ==(const Accelerator& rhs) const {
  if ((key_code_ == rhs.key_code_) && (type_ == rhs.type_) &&
      (modifiers_ == rhs.modifiers_))
    return true;

  bool platform_equal =
      platform_accelerator_.get() && rhs.platform_accelerator_.get() &&
      platform_accelerator_.get() == rhs.platform_accelerator_.get();

  return platform_equal;
}

bool Accelerator::operator !=(const Accelerator& rhs) const {
  return !(*this == rhs);
}

bool Accelerator::IsShiftDown() const {
  return (modifiers_ & EF_SHIFT_DOWN) != 0;
}

bool Accelerator::IsCtrlDown() const {
  return (modifiers_ & EF_CONTROL_DOWN) != 0;
}

bool Accelerator::IsAltDown() const {
  return (modifiers_ & EF_ALT_DOWN) != 0;
}

bool Accelerator::IsCmdDown() const {
  return (modifiers_ & EF_COMMAND_DOWN) != 0;
}

bool Accelerator::IsRepeat() const {
  return is_repeat_;
}

base::string16 Accelerator::GetShortcutText() const {
  int string_id = 0;
  switch (key_code_) {
    case ui::VKEY_TAB:
      string_id = IDS_APP_TAB_KEY;
      break;
    case ui::VKEY_RETURN:
      string_id = IDS_APP_ENTER_KEY;
      break;
    case ui::VKEY_ESCAPE:
      string_id = IDS_APP_ESC_KEY;
      break;
    case ui::VKEY_SPACE:
      string_id = IDS_APP_SPACE_KEY;
      break;
    case ui::VKEY_PRIOR:
      string_id = IDS_APP_PAGEUP_KEY;
      break;
    case ui::VKEY_NEXT:
      string_id = IDS_APP_PAGEDOWN_KEY;
      break;
    case ui::VKEY_END:
      string_id = IDS_APP_END_KEY;
      break;
    case ui::VKEY_HOME:
      string_id = IDS_APP_HOME_KEY;
      break;
    case ui::VKEY_INSERT:
      string_id = IDS_APP_INSERT_KEY;
      break;
    case ui::VKEY_DELETE:
      string_id = IDS_APP_DELETE_KEY;
      break;
    case ui::VKEY_LEFT:
      string_id = IDS_APP_LEFT_ARROW_KEY;
      break;
    case ui::VKEY_RIGHT:
      string_id = IDS_APP_RIGHT_ARROW_KEY;
      break;
    case ui::VKEY_UP:
      string_id = IDS_APP_UP_ARROW_KEY;
      break;
    case ui::VKEY_DOWN:
      string_id = IDS_APP_DOWN_ARROW_KEY;
      break;
    case ui::VKEY_BACK:
      string_id = IDS_APP_BACKSPACE_KEY;
      break;
    case ui::VKEY_F1:
      string_id = IDS_APP_F1_KEY;
      break;
    case ui::VKEY_F11:
      string_id = IDS_APP_F11_KEY;
      break;
    case ui::VKEY_OEM_COMMA:
      string_id = IDS_APP_COMMA_KEY;
      break;
    case ui::VKEY_OEM_PERIOD:
      string_id = IDS_APP_PERIOD_KEY;
      break;
    case ui::VKEY_MEDIA_NEXT_TRACK:
      string_id = IDS_APP_MEDIA_NEXT_TRACK_KEY;
      break;
    case ui::VKEY_MEDIA_PLAY_PAUSE:
      string_id = IDS_APP_MEDIA_PLAY_PAUSE_KEY;
      break;
    case ui::VKEY_MEDIA_PREV_TRACK:
      string_id = IDS_APP_MEDIA_PREV_TRACK_KEY;
      break;
    case ui::VKEY_MEDIA_STOP:
      string_id = IDS_APP_MEDIA_STOP_KEY;
      break;
    default:
      break;
  }

  base::string16 shortcut;
  if (!string_id) {
#if defined(OS_WIN)
    // Our fallback is to try translate the key code to a regular character
    // unless it is one of digits (VK_0 to VK_9). Some keyboard
    // layouts have characters other than digits assigned in
    // an unshifted mode (e.g. French AZERY layout has 'a with grave
    // accent' for '0'). For display in the menu (e.g. Ctrl-0 for the
    // default zoom level), we leave VK_[0-9] alone without translation.
    wchar_t key;
    if (base::IsAsciiDigit(key_code_))
      key = static_cast<wchar_t>(key_code_);
    else
      key = LOWORD(::MapVirtualKeyW(key_code_, MAPVK_VK_TO_CHAR));
    shortcut += key;
#elif defined(USE_AURA) || defined(OS_MACOSX)
    const uint16_t c = DomCodeToUsLayoutCharacter(
        UsLayoutKeyboardCodeToDomCode(key_code_), false);
    if (c != 0)
      shortcut +=
          static_cast<base::string16::value_type>(base::ToUpperASCII(c));
#endif
  } else {
    shortcut = l10n_util::GetStringUTF16(string_id);
  }

  // Checking whether the character used for the accelerator is alphanumeric.
  // If it is not, then we need to adjust the string later on if the locale is
  // right-to-left. See below for more information of why such adjustment is
  // required.
  base::string16 shortcut_rtl;
  bool adjust_shortcut_for_rtl = false;
  if (base::i18n::IsRTL() && shortcut.length() == 1 &&
      !base::IsAsciiAlpha(shortcut[0]) && !base::IsAsciiDigit(shortcut[0])) {
    adjust_shortcut_for_rtl = true;
    shortcut_rtl.assign(shortcut);
  }

  if (IsShiftDown())
    shortcut = l10n_util::GetStringFUTF16(IDS_APP_SHIFT_MODIFIER, shortcut);

  // Note that we use 'else-if' in order to avoid using Ctrl+Alt as a shortcut.
  // See http://blogs.msdn.com/oldnewthing/archive/2004/03/29/101121.aspx for
  // more information.
  if (IsCtrlDown())
    shortcut = l10n_util::GetStringFUTF16(IDS_APP_CONTROL_MODIFIER, shortcut);
  else if (IsAltDown())
    shortcut = l10n_util::GetStringFUTF16(IDS_APP_ALT_MODIFIER, shortcut);

  if (IsCmdDown()) {
#if defined(OS_MACOSX)
    shortcut = l10n_util::GetStringFUTF16(IDS_APP_COMMAND_MODIFIER, shortcut);
#elif defined(OS_CHROMEOS)
    shortcut = l10n_util::GetStringFUTF16(IDS_APP_SEARCH_MODIFIER, shortcut);
#else
    NOTREACHED();
#endif
  }

  // For some reason, menus in Windows ignore standard Unicode directionality
  // marks (such as LRE, PDF, etc.). On RTL locales, we use RTL menus and
  // therefore any text we draw for the menu items is drawn in an RTL context.
  // Thus, the text "Ctrl++" (which we currently use for the Zoom In option)
  // appears as "++Ctrl" in RTL because the Unicode BiDi algorithm puts
  // punctuations on the left when the context is right-to-left. Shortcuts that
  // do not end with a punctuation mark (such as "Ctrl+H" do not have this
  // problem).
  //
  // The only way to solve this problem is to adjust the string if the locale
  // is RTL so that it is drawn correctly in an RTL context. Instead of
  // returning "Ctrl++" in the above example, we return "++Ctrl". This will
  // cause the text to appear as "Ctrl++" when Windows draws the string in an
  // RTL context because the punctuation no longer appears at the end of the
  // string.
  //
  // TODO(idana) bug# 1232732: this hack can be avoided if instead of using
  // views::Menu we use views::MenuItemView because the latter is a View
  // subclass and therefore it supports marking text as RTL or LTR using
  // standard Unicode directionality marks.
  if (adjust_shortcut_for_rtl) {
    int key_length = static_cast<int>(shortcut_rtl.length());
    DCHECK_GT(key_length, 0);
    shortcut_rtl.append(base::ASCIIToUTF16("+"));

    // Subtracting the size of the shortcut key and 1 for the '+' sign.
    shortcut_rtl.append(shortcut, 0, shortcut.length() - key_length - 1);
    shortcut.swap(shortcut_rtl);
  }

  return shortcut;
}

}  // namespace ui
