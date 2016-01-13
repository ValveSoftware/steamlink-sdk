// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_SCROLLBAR_DEV_H_
#define PPAPI_CPP_DEV_SCROLLBAR_DEV_H_

#include "ppapi/c/dev/ppb_scrollbar_dev.h"
#include "ppapi/cpp/dev/widget_dev.h"

namespace pp {

class InstanceHandle;

// This class allows a plugin to use the browser's scrollbar widget.
class Scrollbar_Dev : public Widget_Dev {
 public:
  // Creates an is_null() Scrollbar object.
  Scrollbar_Dev() {}

  explicit Scrollbar_Dev(PP_Resource resource);
  Scrollbar_Dev(const InstanceHandle& instance, bool vertical);
  Scrollbar_Dev(const Scrollbar_Dev& other);

  Scrollbar_Dev& operator=(const Scrollbar_Dev& other);

  // PPB_Scrollbar methods:
  uint32_t GetThickness();
  bool IsOverlay();
  uint32_t GetValue();
  void SetValue(uint32_t value);
  void SetDocumentSize(uint32_t size);
  void SetTickMarks(const Rect* tick_marks, uint32_t count);
  void ScrollBy(PP_ScrollBy_Dev unit, int32_t multiplier);
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_SCROLLBAR_DEV_H_
