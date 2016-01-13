// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "win8/metro_driver/ime/ime_popup_monitor.h"

#include <windows.h>

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "win8/metro_driver/ime/ime_popup_observer.h"

namespace metro_driver {
namespace {

ImePopupObserver* g_observer_ = NULL;
HWINEVENTHOOK g_hook_handle_ = NULL;

void CALLBACK ImeEventCallback(HWINEVENTHOOK win_event_hook_handle,
                               DWORD event,
                               HWND window_handle,
                               LONG object_id,
                               LONG child_id,
                               DWORD event_thread,
                               DWORD event_time) {
  // This function is registered to SetWinEventHook to be called back on the UI
  // thread.
  DCHECK(base::MessageLoopForUI::IsCurrent());

  if (!g_observer_)
    return;
  switch (event) {
    case EVENT_OBJECT_IME_SHOW:
      g_observer_->OnImePopupChanged(ImePopupObserver::kPopupShown);
      return;
    case EVENT_OBJECT_IME_HIDE:
      g_observer_->OnImePopupChanged(ImePopupObserver::kPopupHidden);
      return;
    case EVENT_OBJECT_IME_CHANGE:
      g_observer_->OnImePopupChanged(ImePopupObserver::kPopupUpdated);
      return;
  }
}

}  // namespace

void AddImePopupObserver(ImePopupObserver* observer) {
  CHECK(g_observer_ == NULL)
      << "Currently only one observer is supported at the same time.";
  g_observer_ = observer;

  // IMEs running under immersive mode are supposed to generate WinEvent
  // whenever their popup UI such as candidate window is shown, updated, and
  // hidden to support accessibility applications.
  // http://msdn.microsoft.com/en-us/library/windows/apps/hh967425.aspx#accessibility
  // Note that there is another mechanism in TSF, called ITfUIElementSink, to
  // subscribe when the visibility of an IME's UI element is changed. However,
  // MS-IME running under immersive mode does not fully support this API.
  // Thus, WinEvent is more reliable for this purpose.
  g_hook_handle_ = SetWinEventHook(
      EVENT_OBJECT_IME_SHOW,
      EVENT_OBJECT_IME_CHANGE,
      NULL,
      ImeEventCallback,
      GetCurrentProcessId(),  // monitor the metro_driver process only
      0,   // hook all threads because MS-IME emits WinEvent in a worker thread
      WINEVENT_OUTOFCONTEXT);  // allows us to receive message in the UI thread
  LOG_IF(ERROR, !g_hook_handle_) << "SetWinEventHook failed.";
}

void RemoveImePopupObserver(ImePopupObserver* observer) {
  if (g_observer_ != observer)
    return;
  g_observer_ = NULL;
  if (!g_hook_handle_)
    return;
  const bool unhook_succeeded = !!UnhookWinEvent(g_hook_handle_);
  LOG_IF(ERROR, !unhook_succeeded) << "UnhookWinEvent failed.";
  g_hook_handle_ = NULL;
}

}  // namespace metro_driver
