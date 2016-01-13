// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/npapi/webplugin_ime_win.h"

#include <cstring>
#include <string>
#include <vector>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "content/child/npapi/plugin_instance.h"
#include "content/common/plugin_constants_win.h"

#pragma comment(lib, "imm32.lib")

namespace content {

// A critical section that prevents two or more plug-ins from accessing a
// WebPluginIMEWin instance through our patch function.
base::LazyInstance<base::Lock>::Leaky
    g_webplugin_ime_lock = LAZY_INSTANCE_INITIALIZER;

WebPluginIMEWin* WebPluginIMEWin::instance_ = NULL;

WebPluginIMEWin::WebPluginIMEWin()
    : cursor_position_(0),
      delta_start_(0),
      composing_text_(false),
      support_ime_messages_(false),
      status_updated_(false),
      input_type_(1) {
  memset(result_clauses_, 0, sizeof(result_clauses_));
}

WebPluginIMEWin::~WebPluginIMEWin() {
}

void WebPluginIMEWin::CompositionUpdated(const base::string16& text,
                                         std::vector<int> clauses,
                                         std::vector<int> target,
                                         int cursor_position) {
  // Send a WM_IME_STARTCOMPOSITION message when a user starts a composition.
  NPEvent np_event;
  if (!composing_text_) {
    composing_text_ = true;
    result_text_.clear();

    np_event.event = WM_IME_STARTCOMPOSITION;
    np_event.wParam = 0;
    np_event.lParam = 0;
    events_.push_back(np_event);
  }

  // We can update the following values from this event: GCS_COMPSTR,
  // GCS_COMPATTR, GCSCOMPCLAUSE, GCS_CURSORPOS, and GCS_DELTASTART. We send a
  // WM_IME_COMPOSITION message to notify the list of updated values.
  np_event.event = WM_IME_COMPOSITION;
  np_event.wParam = 0;
  np_event.lParam = GCS_COMPSTR | GCS_COMPATTR | GCS_COMPCLAUSE |
      GCS_CURSORPOS | GCS_DELTASTART;
  events_.push_back(np_event);

  // Converts this event to the IMM32 data so we do not have to convert it every
  // time when a plug-in call an IMM32 function.
  composition_text_ = text;

  // Create the composition clauses returned when a plug-in calls
  // ImmGetCompositionString() with GCS_COMPCLAUSE.
  composition_clauses_.clear();
  for (size_t i = 0; i < clauses.size(); ++i)
    composition_clauses_.push_back(clauses[i]);

  // Create the composition attributes used by GCS_COMPATTR.
  if (target.size() == 2) {
    composition_attributes_.assign(text.length(), ATTR_CONVERTED);
    for (int i = target[0]; i < target[1]; ++i)
        composition_attributes_[i] = ATTR_TARGET_CONVERTED;
  } else {
    composition_attributes_.assign(text.length(), ATTR_INPUT);
  }

  cursor_position_ = cursor_position;
  delta_start_ = cursor_position;
}

void WebPluginIMEWin::CompositionCompleted(const base::string16& text) {
  composing_text_ = false;

  // We should update the following values when we finish a composition:
  // GCS_RESULTSTR, GCS_RESULTCLAUSE, GCS_CURSORPOS, and GCS_DELTASTART. We
  // send a WM_IME_COMPOSITION message to notify the list of updated values.
  NPEvent np_event;
  np_event.event = WM_IME_COMPOSITION;
  np_event.wParam = 0;
  np_event.lParam = GCS_CURSORPOS | GCS_DELTASTART | GCS_RESULTSTR |
      GCS_RESULTCLAUSE;
  events_.push_back(np_event);

  // We also send a WM_IME_ENDCOMPOSITION message after the final
  // WM_IME_COMPOSITION message (i.e. after finishing a composition).
  np_event.event = WM_IME_ENDCOMPOSITION;
  np_event.wParam = 0;
  np_event.lParam = 0;
  events_.push_back(np_event);

  // If the target plug-in does not seem to support IME messages, we send
  // each character in IME text with a WM_CHAR message so the plug-in can
  // insert the IME text.
  if (!support_ime_messages_) {
    np_event.event = WM_CHAR;
    np_event.wParam = 0;
    np_event.lParam = 0;
    for (size_t i = 0; i < result_text_.length(); ++i) {
      np_event.wParam = result_text_[i];
      events_.push_back(np_event);
    }
  }

  // Updated the result text and its clause. (Unlike composition clauses, a
  // result clause consists of only one region.)
  result_text_ = text;

  result_clauses_[0] = 0;
  result_clauses_[1] = result_text_.length();

  cursor_position_ = result_clauses_[1];
  delta_start_ = result_clauses_[1];
}

bool WebPluginIMEWin::SendEvents(PluginInstance* instance) {
  // We allow the patch functions to access this WebPluginIMEWin instance only
  // while we send IME events to the plug-in.
  ScopedLock lock(this);

  bool ret = true;
  for (std::vector<NPEvent>::iterator it = events_.begin();
       it != events_.end(); ++it) {
    if (!instance->NPP_HandleEvent(&(*it)))
      ret = false;
  }

  events_.clear();
  return ret;
}

bool WebPluginIMEWin::GetStatus(int* input_type, gfx::Rect* caret_rect) {
  *input_type = input_type_;
  *caret_rect = caret_rect_;
  return true;
}

// static
FARPROC WebPluginIMEWin::GetProcAddress(LPCSTR name) {
  static const struct {
    const char* name;
    FARPROC function;
  } kImm32Functions[] = {
    { "ImmAssociateContextEx",
        reinterpret_cast<FARPROC>(ImmAssociateContextEx) },
    { "ImmGetCompositionStringW",
        reinterpret_cast<FARPROC>(ImmGetCompositionStringW) },
    { "ImmGetContext", reinterpret_cast<FARPROC>(ImmGetContext) },
    { "ImmReleaseContext", reinterpret_cast<FARPROC>(ImmReleaseContext) },
    { "ImmSetCandidateWindow",
        reinterpret_cast<FARPROC>(ImmSetCandidateWindow) },
    { "ImmSetOpenStatus", reinterpret_cast<FARPROC>(ImmSetOpenStatus) },
  };
  for (int i = 0; i < arraysize(kImm32Functions); ++i) {
    if (!lstrcmpiA(name, kImm32Functions[i].name))
      return kImm32Functions[i].function;
  }
  return NULL;
}

void WebPluginIMEWin::Lock() {
  g_webplugin_ime_lock.Pointer()->Acquire();
  instance_ = this;
}

void WebPluginIMEWin::Unlock() {
  instance_ = NULL;
  g_webplugin_ime_lock.Pointer()->Release();
}

// static
WebPluginIMEWin* WebPluginIMEWin::GetInstance(HIMC context) {
  return instance_ && context == reinterpret_cast<HIMC>(instance_) ?
      instance_ : NULL;
}

// static
BOOL WINAPI WebPluginIMEWin::ImmAssociateContextEx(HWND window,
                                                   HIMC context,
                                                   DWORD flags) {
  WebPluginIMEWin* instance = GetInstance(context);
  if (!instance)
    return ::ImmAssociateContextEx(window, context, flags);

  int input_type = !context && !flags;
  instance->input_type_ = input_type;
  instance->status_updated_ = true;
  return TRUE;
}

// static
LONG WINAPI WebPluginIMEWin::ImmGetCompositionStringW(HIMC context,
                                                      DWORD index,
                                                      LPVOID dst_data,
                                                      DWORD dst_size) {
  WebPluginIMEWin* instance = GetInstance(context);
  if (!instance)
    return ::ImmGetCompositionStringW(context, index, dst_data, dst_size);

  const void* src_data = NULL;
  DWORD src_size = 0;
  switch (index) {
    case GCS_COMPSTR:
      src_data = instance->composition_text_.c_str();
      src_size = instance->composition_text_.length() * sizeof(wchar_t);
      break;

    case GCS_COMPATTR:
      src_data = instance->composition_attributes_.c_str();
      src_size = instance->composition_attributes_.length();
      break;

    case GCS_COMPCLAUSE:
      src_data = &instance->composition_clauses_[0];
      src_size = instance->composition_clauses_.size() * sizeof(uint32);
      break;

    case GCS_CURSORPOS:
      return instance->cursor_position_;

    case GCS_DELTASTART:
      return instance->delta_start_;

    case GCS_RESULTSTR:
      src_data = instance->result_text_.c_str();
      src_size = instance->result_text_.length() * sizeof(wchar_t);
      break;

    case GCS_RESULTCLAUSE:
      src_data = &instance->result_clauses_[0];
      src_size = sizeof(instance->result_clauses_);
      break;

    default:
      break;
  }
  if (!src_data || !src_size)
    return IMM_ERROR_NODATA;

  if (dst_size >= src_size)
    memcpy(dst_data, src_data, src_size);

  return src_size;
}

// static
HIMC WINAPI WebPluginIMEWin::ImmGetContext(HWND window) {
  // Call the original ImmGetContext() function if the given window is the one
  // created in WebPluginDelegateImpl::WindowedCreatePlugin(). (We attached IME
  // context only with the windows created in this function.) On the other hand,
  // some windowless plug-ins (such as Flash) call this function with a dummy
  // window handle. We return our dummy IME context for these plug-ins so they
  // can use our IME emulator.
  if (IsWindow(window)) {
    wchar_t name[128];
    GetClassName(window, &name[0], arraysize(name));
    if (!wcscmp(&name[0], kNativeWindowClassName))
      return ::ImmGetContext(window);
  }

  WebPluginIMEWin* instance = instance_;
  if (instance)
    instance->support_ime_messages_ = true;
  return reinterpret_cast<HIMC>(instance);
}

// static
BOOL WINAPI WebPluginIMEWin::ImmReleaseContext(HWND window, HIMC context) {
  if (!GetInstance(context))
    return ::ImmReleaseContext(window, context);
  return TRUE;
}

// static
BOOL WINAPI WebPluginIMEWin::ImmSetCandidateWindow(HIMC context,
                                                   CANDIDATEFORM* candidate) {
  WebPluginIMEWin* instance = GetInstance(context);
  if (!instance)
    return ::ImmSetCandidateWindow(context, candidate);

  gfx::Rect caret_rect(candidate->rcArea);
  if ((candidate->dwStyle & CFS_EXCLUDE) &&
      instance->caret_rect_ != caret_rect) {
    instance->caret_rect_ = caret_rect;
    instance->status_updated_ = true;
  }
  return TRUE;
}

// static
BOOL WINAPI WebPluginIMEWin::ImmSetOpenStatus(HIMC context, BOOL open) {
  WebPluginIMEWin* instance = GetInstance(context);
  if (!instance)
    return ::ImmSetOpenStatus(context, open);

  int input_type = open ? 1 : 0;
  if (instance->input_type_ != input_type) {
    instance->input_type_ = input_type;
    instance->status_updated_ = true;
  }

  return TRUE;
}

}  // namespace content
