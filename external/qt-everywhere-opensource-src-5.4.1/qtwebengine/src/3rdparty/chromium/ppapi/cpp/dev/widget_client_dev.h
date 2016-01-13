// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_WIDGET_CLIENT_DEV_H_
#define PPAPI_CPP_DEV_WIDGET_CLIENT_DEV_H_

#include "ppapi/c/pp_stdint.h"
#include "ppapi/cpp/instance_handle.h"

namespace pp {

class Instance;
class Rect;
class Scrollbar_Dev;
class Widget_Dev;

// This class provides a C++ interface for callbacks related to widgets. You
// would normally use multiple inheritance to derive from this class in your
// instance.
class WidgetClient_Dev {
 public:
  explicit WidgetClient_Dev(Instance* instance);
  virtual ~WidgetClient_Dev();

  /**
   * Notification that the given widget should be repainted. This is the
   * implementation for PPP_Widget_Dev.
   */
  virtual void InvalidateWidget(Widget_Dev widget, const Rect& dirty_rect) = 0;

  /**
   * Notification that the given scrollbar should change value. This is the
   * implementation for PPP_Scrollbar_Dev.
   */
  virtual void ScrollbarValueChanged(Scrollbar_Dev scrollbar,
                                     uint32_t value) = 0;

  /**
   * Notification that the given scrollbar's overlay type has changed. This is
   * the implementation for PPP_Scrollbar_Dev.
   */
  virtual void ScrollbarOverlayChanged(Scrollbar_Dev scrollbar,
                                       bool type) = 0;

 private:
  InstanceHandle associated_instance_;
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_WIDGET_CLIENT_DEV_H_
