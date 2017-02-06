// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_WINDOW_EVENT_TARGET_H_
#define UI_BASE_WIN_WINDOW_EVENT_TARGET_H_

#include <windows.h>

#include "ui/base/ui_base_export.h"

namespace ui {

// This interface is implemented by classes who get input events forwarded to
// them from others. E.g. would be a win32 parent child relationship where the
// child forwards input events to the parent after doing minimal processing.
class UI_BASE_EXPORT WindowEventTarget {
 public:
  static const char kWin32InputEventTarget[];

  // Handles mouse events like WM_MOUSEMOVE, WM_LBUTTONDOWN, etc.
  // The |message| parameter identifies the message.
  // The |w_param| and |l_param| values are dependent on the type of the
  // message.
  // The |handled| parameter is an output parameter which when set to false
  // indicates that the message should be DefProc'ed.
  // Returns the result of processing the message.
  virtual LRESULT HandleMouseMessage(unsigned int message,
                                     WPARAM w_param,
                                     LPARAM l_param,
                                     bool* handled) = 0;

  // Handles keyboard events like WM_KEYDOWN/WM_KEYUP, etc.
  // The |message| parameter identifies the message.
  // The |w_param| and |l_param| values are dependent on the type of the
  // message.
  // The |handled| parameter is an output parameter which when set to false
  // indicates that the message should be DefProc'ed.
  // Returns the result of processing the message.
  virtual LRESULT HandleKeyboardMessage(unsigned int message,
                                        WPARAM w_param,
                                        LPARAM l_param,
                                        bool* handled) = 0;

  // Handles WM_TOUCH events.
  // The |message| parameter identifies the message.
  // The |w_param| and |l_param| values are as per MSDN docs.
  // The |handled| parameter is an output parameter which when set to false
  // indicates that the message should be DefProc'ed.
  // Returns the result of processing the message.
  virtual LRESULT HandleTouchMessage(unsigned int message,
                                     WPARAM w_param,
                                     LPARAM l_param,
                                     bool* handled) = 0;

  // Handles scroll messages like WM_VSCROLL and WM_HSCROLL.
  // The |message| parameter identifies the scroll message.
  // The |w_param| and |l_param| values are dependent on the type of scroll.
  // The |handled| parameter is an output parameter which when set to false
  // indicates that the message should be DefProc'ed.
  virtual LRESULT HandleScrollMessage(unsigned int message,
                                      WPARAM w_param,
                                      LPARAM l_param,
                                      bool* handled) = 0;

  // Handles the WM_NCHITTEST message
  // The |message| parameter identifies the message.
  // The |w_param| and |l_param| values are as per MSDN docs.
  // The |handled| parameter is an output parameter which when set to false
  // indicates that the message should be DefProc'ed.
  // Returns the result of processing the message.
  virtual LRESULT HandleNcHitTestMessage(unsigned int message,
                                         WPARAM w_param,
                                         LPARAM l_param,
                                         bool* handled) = 0;

  // Notification from the forwarder window that its parent changed.
  virtual void HandleParentChanged() = 0;

 protected:
  WindowEventTarget();
  virtual ~WindowEventTarget();
};

}  // namespace ui

#endif  // UI_BASE_WIN_WINDOW_EVENT_TARGET_H_


