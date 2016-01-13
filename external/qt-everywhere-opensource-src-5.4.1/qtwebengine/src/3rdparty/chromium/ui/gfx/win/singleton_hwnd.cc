// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/win/singleton_hwnd.h"

#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"

namespace gfx {

// static
SingletonHwnd* SingletonHwnd::GetInstance() {
  return Singleton<SingletonHwnd>::get();
}

void SingletonHwnd::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void SingletonHwnd::RemoveObserver(Observer* observer) {
  if (!hwnd())
    return;
  observer_list_.RemoveObserver(observer);
}

BOOL SingletonHwnd::ProcessWindowMessage(HWND window,
                                         UINT message,
                                         WPARAM wparam,
                                         LPARAM lparam,
                                         LRESULT& result,
                                         DWORD msg_map_id) {
  FOR_EACH_OBSERVER(Observer,
                    observer_list_,
                    OnWndProc(window, message, wparam, lparam));
  return false;
}

SingletonHwnd::SingletonHwnd() {
  if (!base::MessageLoopForUI::IsCurrent()) {
    // Creating this window in (e.g.) a renderer inhibits shutdown on
    // Windows. See http://crbug.com/230122 and http://crbug.com/236039.
    DLOG(ERROR) << "Cannot create windows on non-UI thread!";
    return;
  }
  WindowImpl::Init(NULL, Rect());
}

SingletonHwnd::~SingletonHwnd() {
}

}  // namespace gfx
