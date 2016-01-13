// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/win/imm32_manager.h"

#include <msctf.h>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_comptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ime/composition_text.h"

// "imm32.lib" is required by IMM32 APIs used in this file.
// NOTE(hbono): To comply with a comment from Darin, I have added
// this #pragma directive instead of adding "imm32.lib" to a project file.
#pragma comment(lib, "imm32.lib")

// Following code requires wchar_t to be same as char16. It should always be
// true on Windows.
COMPILE_ASSERT(sizeof(wchar_t) == sizeof(base::char16), wchar_t__char16_diff);

///////////////////////////////////////////////////////////////////////////////
// IMM32Manager

namespace {

// Determines whether or not the given attribute represents a target
// (a.k.a. a selection).
bool IsTargetAttribute(char attribute) {
  return (attribute == ATTR_TARGET_CONVERTED ||
          attribute == ATTR_TARGET_NOTCONVERTED);
}

// Helper function for IMM32Manager::GetCompositionInfo() method, to get the
// target range that's selected by the user in the current composition string.
void GetCompositionTargetRange(HIMC imm_context, int* target_start,
                               int* target_end) {
  int attribute_size = ::ImmGetCompositionString(imm_context, GCS_COMPATTR,
                                                 NULL, 0);
  if (attribute_size > 0) {
    int start = 0;
    int end = 0;
    scoped_ptr<char[]> attribute_data(new char[attribute_size]);
    if (attribute_data.get()) {
      ::ImmGetCompositionString(imm_context, GCS_COMPATTR,
                                attribute_data.get(), attribute_size);
      for (start = 0; start < attribute_size; ++start) {
        if (IsTargetAttribute(attribute_data[start]))
          break;
      }
      for (end = start; end < attribute_size; ++end) {
        if (!IsTargetAttribute(attribute_data[end]))
          break;
      }
    }
    *target_start = start;
    *target_end = end;
  }
}

// Helper function for IMM32Manager::GetCompositionInfo() method, to get
// underlines information of the current composition string.
void GetCompositionUnderlines(HIMC imm_context,
                              int target_start,
                              int target_end,
                              ui::CompositionUnderlines* underlines) {
  int clause_size = ::ImmGetCompositionString(imm_context, GCS_COMPCLAUSE,
                                              NULL, 0);
  int clause_length = clause_size / sizeof(uint32);
  if (clause_length) {
    scoped_ptr<uint32[]> clause_data(new uint32[clause_length]);
    if (clause_data.get()) {
      ::ImmGetCompositionString(imm_context, GCS_COMPCLAUSE,
                                clause_data.get(), clause_size);
      for (int i = 0; i < clause_length - 1; ++i) {
        ui::CompositionUnderline underline;
        underline.start_offset = clause_data[i];
        underline.end_offset = clause_data[i+1];
        underline.color = SK_ColorBLACK;
        underline.thick = false;
        underline.background_color = SK_ColorTRANSPARENT;

        // Use thick underline for the target clause.
        if (underline.start_offset >= static_cast<uint32>(target_start) &&
            underline.end_offset <= static_cast<uint32>(target_end)) {
          underline.thick = true;
        }
        underlines->push_back(underline);
      }
    }
  }
}

// Checks if a given primary language ID is a RTL language.
bool IsRTLPrimaryLangID(LANGID lang) {
  switch (lang) {
    case LANG_ARABIC:
    case LANG_HEBREW:
    case LANG_PERSIAN:
    case LANG_SYRIAC:
    case LANG_UIGHUR:
    case LANG_URDU:
      return true;
    default:
      return false;
  }
}

}  // namespace

namespace ui {

IMM32Manager::IMM32Manager()
    : ime_status_(false),
      input_language_id_(LANG_USER_DEFAULT),
      is_composing_(false),
      system_caret_(false),
      caret_rect_(-1, -1, 0, 0),
      use_composition_window_(false) {
}

IMM32Manager::~IMM32Manager() {
}

bool IMM32Manager::SetInputLanguage() {
  // Retrieve the current keyboard layout from Windows and determine whether
  // or not the current input context has IMEs.
  // Also save its input language for language-specific operations required
  // while composing a text.
  HKL keyboard_layout = ::GetKeyboardLayout(0);
  input_language_id_ = reinterpret_cast<LANGID>(keyboard_layout);

  // Check TSF Input Processor first.
  // If the active profile is TSF INPUTPROCESSOR, this is IME.
  base::win::ScopedComPtr<ITfInputProcessorProfileMgr> prof_mgr;
  TF_INPUTPROCESSORPROFILE prof;
  if (SUCCEEDED(prof_mgr.CreateInstance(CLSID_TF_InputProcessorProfiles)) &&
      SUCCEEDED(prof_mgr->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD, &prof)) &&
      prof.hkl == NULL &&
      prof.dwProfileType == TF_PROFILETYPE_INPUTPROCESSOR) {
      ime_status_ = true;
  } else {
    // If the curent language is not using TSF, check IMM32 based IMEs.
    // As ImmIsIME always returns non-0 value on Vista+, use ImmGetIMEFileName
    // instead to check if this HKL has any associated IME file.
    ime_status_ = (ImmGetIMEFileName(keyboard_layout, NULL, 0) != 0);
  }

  return ime_status_;
}

void IMM32Manager::CreateImeWindow(HWND window_handle) {
  // When a user disables TSF (Text Service Framework) and CUAS (Cicero
  // Unaware Application Support), Chinese IMEs somehow ignore function calls
  // to ::ImmSetCandidateWindow(), i.e. they do not move their candidate
  // window to the position given as its parameters, and use the position
  // of the current system caret instead, i.e. it uses ::GetCaretPos() to
  // retrieve the position of their IME candidate window.
  // Therefore, we create a temporary system caret for Chinese IMEs and use
  // it during this input context.
  // Since some third-party Japanese IME also uses ::GetCaretPos() to determine
  // their window position, we also create a caret for Japanese IMEs.
  if (PRIMARYLANGID(input_language_id_) == LANG_CHINESE ||
      PRIMARYLANGID(input_language_id_) == LANG_JAPANESE) {
    if (!system_caret_) {
      if (::CreateCaret(window_handle, NULL, 1, 1)) {
        system_caret_ = true;
      }
    }
  }
  // Restore the positions of the IME windows.
  UpdateImeWindow(window_handle);
}

LRESULT IMM32Manager::SetImeWindowStyle(HWND window_handle, UINT message,
                                    WPARAM wparam, LPARAM lparam,
                                    BOOL* handled) {
  // To prevent the IMM (Input Method Manager) from displaying the IME
  // composition window, Update the styles of the IME windows and EXPLICITLY
  // call ::DefWindowProc() here.
  // NOTE(hbono): We can NEVER let WTL call ::DefWindowProc() when we update
  // the styles of IME windows because the 'lparam' variable is a local one
  // and all its updates disappear in returning from this function, i.e. WTL
  // does not call ::DefWindowProc() with our updated 'lparam' value but call
  // the function with its original value and over-writes our window styles.
  *handled = TRUE;
  lparam &= ~ISC_SHOWUICOMPOSITIONWINDOW;
  return ::DefWindowProc(window_handle, message, wparam, lparam);
}

void IMM32Manager::DestroyImeWindow(HWND window_handle) {
  // Destroy the system caret if we have created for this IME input context.
  if (system_caret_) {
    ::DestroyCaret();
    system_caret_ = false;
  }
}

void IMM32Manager::MoveImeWindow(HWND window_handle, HIMC imm_context) {
  // Does nothing when the target window has no input focus. This is important
  // because the renderer may issue SelectionBoundsChanged event even when it
  // has no input focus. (e.g. the page update caused by incremental search.)
  // So this event should be ignored when the |window_handle| no longer has the
  // input focus.
  if (GetFocus() != window_handle)
    return;

  int x = caret_rect_.x();
  int y = caret_rect_.y();

  const int kCaretMargin = 1;
  if (!use_composition_window_ &&
      PRIMARYLANGID(input_language_id_) == LANG_CHINESE) {
    // As written in a comment in IMM32Manager::CreateImeWindow(),
    // Chinese IMEs ignore function calls to ::ImmSetCandidateWindow()
    // when a user disables TSF (Text Service Framework) and CUAS (Cicero
    // Unaware Application Support).
    // On the other hand, when a user enables TSF and CUAS, Chinese IMEs
    // ignore the position of the current system caret and uses the
    // parameters given to ::ImmSetCandidateWindow() with its 'dwStyle'
    // parameter CFS_CANDIDATEPOS.
    // Therefore, we do not only call ::ImmSetCandidateWindow() but also
    // set the positions of the temporary system caret if it exists.
    CANDIDATEFORM candidate_position = {0, CFS_CANDIDATEPOS, {x, y},
                                        {0, 0, 0, 0}};
    ::ImmSetCandidateWindow(imm_context, &candidate_position);
  }
  if (system_caret_) {
    switch (PRIMARYLANGID(input_language_id_)) {
      case LANG_JAPANESE:
        ::SetCaretPos(x, y + caret_rect_.height());
        break;
      default:
        ::SetCaretPos(x, y);
        break;
    }
  }
  if (use_composition_window_) {
    // Moves the composition text window.
    COMPOSITIONFORM cf = {CFS_POINT, {x, y}};
    ::ImmSetCompositionWindow(imm_context, &cf);
    // Don't need to set the position of candidate window.
    return;
  }

  if (PRIMARYLANGID(input_language_id_) == LANG_KOREAN) {
    // Chinese IMEs and Japanese IMEs require the upper-left corner of
    // the caret to move the position of their candidate windows.
    // On the other hand, Korean IMEs require the lower-left corner of the
    // caret to move their candidate windows.
    y += kCaretMargin;
  }
  // Japanese IMEs and Korean IMEs also use the rectangle given to
  // ::ImmSetCandidateWindow() with its 'dwStyle' parameter CFS_EXCLUDE
  // to move their candidate windows when a user disables TSF and CUAS.
  // Therefore, we also set this parameter here.
  CANDIDATEFORM exclude_rectangle = {0, CFS_EXCLUDE, {x, y},
      {x, y, x + caret_rect_.width(), y + caret_rect_.height()}};
  ::ImmSetCandidateWindow(imm_context, &exclude_rectangle);
}

void IMM32Manager::UpdateImeWindow(HWND window_handle) {
  // Just move the IME window attached to the given window.
  if (caret_rect_.x() >= 0 && caret_rect_.y() >= 0) {
    HIMC imm_context = ::ImmGetContext(window_handle);
    if (imm_context) {
      MoveImeWindow(window_handle, imm_context);
      ::ImmReleaseContext(window_handle, imm_context);
    }
  }
}

void IMM32Manager::CleanupComposition(HWND window_handle) {
  // Notify the IMM attached to the given window to complete the ongoing
  // composition, (this case happens when the given window is de-activated
  // while composing a text and re-activated), and reset the omposition status.
  if (is_composing_) {
    HIMC imm_context = ::ImmGetContext(window_handle);
    if (imm_context) {
      ::ImmNotifyIME(imm_context, NI_COMPOSITIONSTR, CPS_COMPLETE, 0);
      ::ImmReleaseContext(window_handle, imm_context);
    }
    ResetComposition(window_handle);
  }
}

void IMM32Manager::ResetComposition(HWND window_handle) {
  // Currently, just reset the composition status.
  is_composing_ = false;
}

void IMM32Manager::CompleteComposition(HWND window_handle, HIMC imm_context) {
  // We have to confirm there is an ongoing composition before completing it.
  // This is for preventing some IMEs from getting confused while completing an
  // ongoing composition even if they do not have any ongoing compositions.)
  if (is_composing_) {
    ::ImmNotifyIME(imm_context, NI_COMPOSITIONSTR, CPS_COMPLETE, 0);
    ResetComposition(window_handle);
  }
}

void IMM32Manager::GetCompositionInfo(HIMC imm_context, LPARAM lparam,
                                  CompositionText* composition) {
  // We only care about GCS_COMPATTR, GCS_COMPCLAUSE and GCS_CURSORPOS, and
  // convert them into underlines and selection range respectively.
  composition->underlines.clear();

  int length = static_cast<int>(composition->text.length());

  // Find out the range selected by the user.
  int target_start = length;
  int target_end = length;
  if (lparam & GCS_COMPATTR)
    GetCompositionTargetRange(imm_context, &target_start, &target_end);

  // Retrieve the selection range information. If CS_NOMOVECARET is specified,
  // that means the cursor should not be moved, then we just place the caret at
  // the beginning of the composition string. Otherwise we should honour the
  // GCS_CURSORPOS value if it's available.
  // TODO(suzhe): due to a bug of webkit, we currently can't use selection range
  // with composition string. See: https://bugs.webkit.org/show_bug.cgi?id=40805
  if (!(lparam & CS_NOMOVECARET) && (lparam & GCS_CURSORPOS)) {
    // IMM32 does not support non-zero-width selection in a composition. So
    // always use the caret position as selection range.
    int cursor = ::ImmGetCompositionString(imm_context, GCS_CURSORPOS, NULL, 0);
    composition->selection = gfx::Range(cursor);
  } else {
    composition->selection = gfx::Range(0);
  }

  // Retrieve the clause segmentations and convert them to underlines.
  if (lparam & GCS_COMPCLAUSE) {
    GetCompositionUnderlines(imm_context, target_start, target_end,
                             &composition->underlines);
  }

  // Set default underlines in case there is no clause information.
  if (!composition->underlines.size()) {
    CompositionUnderline underline;
    underline.color = SK_ColorBLACK;
    underline.background_color = SK_ColorTRANSPARENT;
    if (target_start > 0) {
      underline.start_offset = 0U;
      underline.end_offset = static_cast<uint32>(target_start);
      underline.thick = false;
      composition->underlines.push_back(underline);
    }
    if (target_end > target_start) {
      underline.start_offset = static_cast<uint32>(target_start);
      underline.end_offset = static_cast<uint32>(target_end);
      underline.thick = true;
      composition->underlines.push_back(underline);
    }
    if (target_end < length) {
      underline.start_offset = static_cast<uint32>(target_end);
      underline.end_offset = static_cast<uint32>(length);
      underline.thick = false;
      composition->underlines.push_back(underline);
    }
  }
}

bool IMM32Manager::GetString(HIMC imm_context,
                         WPARAM lparam,
                         int type,
                         base::string16* result) {
  if (!(lparam & type))
    return false;
  LONG string_size = ::ImmGetCompositionString(imm_context, type, NULL, 0);
  if (string_size <= 0)
    return false;
  DCHECK_EQ(0u, string_size % sizeof(wchar_t));
  ::ImmGetCompositionString(imm_context, type,
      WriteInto(result, (string_size / sizeof(wchar_t)) + 1), string_size);
  return true;
}

bool IMM32Manager::GetResult(
    HWND window_handle, LPARAM lparam, base::string16* result) {
  bool ret = false;
  HIMC imm_context = ::ImmGetContext(window_handle);
  if (imm_context) {
    ret = GetString(imm_context, lparam, GCS_RESULTSTR, result);
    ::ImmReleaseContext(window_handle, imm_context);
  }
  return ret;
}

bool IMM32Manager::GetComposition(HWND window_handle, LPARAM lparam,
                              CompositionText* composition) {
  bool ret = false;
  HIMC imm_context = ::ImmGetContext(window_handle);
  if (imm_context) {
    // Copy the composition string to the CompositionText object.
    ret = GetString(imm_context, lparam, GCS_COMPSTR, &composition->text);

    if (ret) {
      // This is a dirty workaround for facebook. Facebook deletes the
      // placeholder character (U+3000) used by Traditional-Chinese IMEs at the
      // beginning of composition text. This prevents WebKit from replacing this
      // placeholder character with a Traditional-Chinese character, i.e. we
      // cannot input any characters in a comment box of facebook with
      // Traditional-Chinese IMEs. As a workaround, we replace U+3000 at the
      // beginning of composition text with U+FF3F, a placeholder character used
      // by Japanese IMEs.
      if (input_language_id_ == MAKELANGID(LANG_CHINESE,
                                           SUBLANG_CHINESE_TRADITIONAL) &&
          composition->text[0] == 0x3000) {
        composition->text[0] = 0xFF3F;
      }

      // Retrieve the composition underlines and selection range information.
      GetCompositionInfo(imm_context, lparam, composition);

      // Mark that there is an ongoing composition.
      is_composing_ = true;
    }

    ::ImmReleaseContext(window_handle, imm_context);
  }
  return ret;
}

void IMM32Manager::DisableIME(HWND window_handle) {
  // A renderer process have moved its input focus to a password input
  // when there is an ongoing composition, e.g. a user has clicked a
  // mouse button and selected a password input while composing a text.
  // For this case, we have to complete the ongoing composition and
  // clean up the resources attached to this object BEFORE DISABLING THE IME.
  CleanupComposition(window_handle);
  ::ImmAssociateContextEx(window_handle, NULL, 0);
}

void IMM32Manager::CancelIME(HWND window_handle) {
  if (is_composing_) {
    HIMC imm_context = ::ImmGetContext(window_handle);
    if (imm_context) {
      ::ImmNotifyIME(imm_context, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
      ::ImmReleaseContext(window_handle, imm_context);
    }
    ResetComposition(window_handle);
  }
}

void IMM32Manager::EnableIME(HWND window_handle) {
  // Load the default IME context.
  // NOTE(hbono)
  //   IMM ignores this call if the IME context is loaded. Therefore, we do
  //   not have to check whether or not the IME context is loaded.
  ::ImmAssociateContextEx(window_handle, NULL, IACE_DEFAULT);
}

void IMM32Manager::UpdateCaretRect(HWND window_handle,
                               const gfx::Rect& caret_rect) {
  // Save the caret position, and Update the position of the IME window.
  // This update is used for moving an IME window when a renderer process
  // resize/moves the input caret.
  if (caret_rect_ != caret_rect) {
    caret_rect_ = caret_rect;
    // Move the IME windows.
    HIMC imm_context = ::ImmGetContext(window_handle);
    if (imm_context) {
      MoveImeWindow(window_handle, imm_context);
      ::ImmReleaseContext(window_handle, imm_context);
    }
  }
}

void IMM32Manager::SetUseCompositionWindow(bool use_composition_window) {
  use_composition_window_ = use_composition_window;
}

std::string IMM32Manager::GetInputLanguageName() const {
  const LCID locale_id = MAKELCID(input_language_id_, SORT_DEFAULT);
  // max size for LOCALE_SISO639LANGNAME and LOCALE_SISO3166CTRYNAME is 9.
  wchar_t buffer[9];

  // Get language id.
  int length = ::GetLocaleInfo(locale_id, LOCALE_SISO639LANGNAME, &buffer[0],
                               arraysize(buffer));
  if (length <= 1)
    return std::string();

  std::string language;
  base::WideToUTF8(buffer, length - 1, &language);
  if (SUBLANGID(input_language_id_) == SUBLANG_NEUTRAL)
    return language;

  // Get region id.
  length = ::GetLocaleInfo(locale_id, LOCALE_SISO3166CTRYNAME, &buffer[0],
                           arraysize(buffer));
  if (length <= 1)
    return language;

  std::string region;
  base::WideToUTF8(buffer, length - 1, &region);
  return language.append(1, '-').append(region);
}

void IMM32Manager::SetTextInputMode(HWND window_handle,
                                    TextInputMode input_mode) {
  if (input_mode == ui::TEXT_INPUT_MODE_DEFAULT)
    return;

  const HIMC imm_context = ::ImmGetContext(window_handle);
  if (!imm_context)
    return;

  DWORD conversion_mode = 0;
  DWORD sentence_mode = 0;
  if (::ImmGetConversionStatus(imm_context, &conversion_mode, &sentence_mode)
      == FALSE) {
    return;
  }

  BOOL open = FALSE;
  ConvertInputModeToImmFlags(input_mode, conversion_mode, &open,
                             &conversion_mode),

  ::ImmSetOpenStatus(imm_context, open);
  if (open)
    ::ImmSetConversionStatus(imm_context, conversion_mode, sentence_mode);
  ::ImmReleaseContext(window_handle, imm_context);
}

// static
bool IMM32Manager::IsRTLKeyboardLayoutInstalled() {
  static enum {
    RTL_KEYBOARD_LAYOUT_NOT_INITIALIZED,
    RTL_KEYBOARD_LAYOUT_INSTALLED,
    RTL_KEYBOARD_LAYOUT_NOT_INSTALLED,
    RTL_KEYBOARD_LAYOUT_ERROR,
  } layout = RTL_KEYBOARD_LAYOUT_NOT_INITIALIZED;

  // Cache the result value.
  if (layout != RTL_KEYBOARD_LAYOUT_NOT_INITIALIZED)
    return layout == RTL_KEYBOARD_LAYOUT_INSTALLED;

  // Retrieve the number of layouts installed in this system.
  int size = GetKeyboardLayoutList(0, NULL);
  if (size <= 0) {
    layout = RTL_KEYBOARD_LAYOUT_ERROR;
    return false;
  }

  // Retrieve the keyboard layouts in an array and check if there is an RTL
  // layout in it.
  scoped_ptr<HKL[]> layouts(new HKL[size]);
  ::GetKeyboardLayoutList(size, layouts.get());
  for (int i = 0; i < size; ++i) {
    if (IsRTLPrimaryLangID(PRIMARYLANGID(layouts[i]))) {
      layout = RTL_KEYBOARD_LAYOUT_INSTALLED;
      return true;
    }
  }

  layout = RTL_KEYBOARD_LAYOUT_NOT_INSTALLED;
  return false;
}

bool IMM32Manager::IsCtrlShiftPressed(base::i18n::TextDirection* direction) {
  uint8_t keystate[256];
  if (!::GetKeyboardState(&keystate[0]))
    return false;

  // To check if a user is pressing only a control key and a right-shift key
  // (or a left-shift key), we use the steps below:
  // 1. Check if a user is pressing a control key and a right-shift key (or
  //    a left-shift key).
  // 2. If the condition 1 is true, we should check if there are any other
  //    keys pressed at the same time.
  //    To ignore the keys checked in 1, we set their status to 0 before
  //    checking the key status.
  const int kKeyDownMask = 0x80;
  if ((keystate[VK_CONTROL] & kKeyDownMask) == 0)
    return false;

  if (keystate[VK_RSHIFT] & kKeyDownMask) {
    keystate[VK_RSHIFT] = 0;
    *direction = base::i18n::RIGHT_TO_LEFT;
  } else if (keystate[VK_LSHIFT] & kKeyDownMask) {
    keystate[VK_LSHIFT] = 0;
    *direction = base::i18n::LEFT_TO_RIGHT;
  } else {
    return false;
  }

  // Scan the key status to find pressed keys. We should abandon changing the
  // text direction when there are other pressed keys.
  // This code is executed only when a user is pressing a control key and a
  // right-shift key (or a left-shift key), i.e. we should ignore the status of
  // the keys: VK_SHIFT, VK_CONTROL, VK_RCONTROL, and VK_LCONTROL.
  // So, we reset their status to 0 and ignore them.
  keystate[VK_SHIFT] = 0;
  keystate[VK_CONTROL] = 0;
  keystate[VK_RCONTROL] = 0;
  keystate[VK_LCONTROL] = 0;
  // Oddly, pressing F10 in another application seemingly breaks all subsequent
  // calls to GetKeyboardState regarding the state of the F22 key. Perhaps this
  // defect is limited to my keyboard driver, but ignoring F22 should be okay.
  keystate[VK_F22] = 0;
  for (int i = 0; i <= VK_PACKET; ++i) {
    if (keystate[i] & kKeyDownMask)
      return false;
  }
  return true;
}

void IMM32Manager::ConvertInputModeToImmFlags(TextInputMode input_mode,
                                              DWORD initial_conversion_mode,
                                              BOOL* open,
                                              DWORD* new_conversion_mode) {
  *open = TRUE;
  *new_conversion_mode = initial_conversion_mode;
  switch (input_mode) {
    case ui::TEXT_INPUT_MODE_FULL_WIDTH_LATIN:
      *new_conversion_mode |= IME_CMODE_FULLSHAPE;
      *new_conversion_mode &= ~(IME_CMODE_NATIVE
                              | IME_CMODE_KATAKANA);
      break;
    case ui::TEXT_INPUT_MODE_KANA:
      *new_conversion_mode |= (IME_CMODE_NATIVE
                             | IME_CMODE_FULLSHAPE);
      *new_conversion_mode &= ~IME_CMODE_KATAKANA;
      break;
    case ui::TEXT_INPUT_MODE_KATAKANA:
      *new_conversion_mode |= (IME_CMODE_NATIVE
                             | IME_CMODE_KATAKANA
                             | IME_CMODE_FULLSHAPE);
      break;
    default:
      *open = FALSE;
      break;
  }
}

}  // namespace ui
