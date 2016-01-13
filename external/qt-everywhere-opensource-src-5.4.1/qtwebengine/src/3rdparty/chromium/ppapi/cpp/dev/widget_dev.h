// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_WIDGET_DEV_H_
#define PPAPI_CPP_DEV_WIDGET_DEV_H_

#include "ppapi/c/pp_stdint.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class ImageData;
class InputEvent;
class Rect;

// This is the base class for widget elements.  As such, it can't be created
// directly.
class Widget_Dev : public Resource {
 public:
  // Creates an is_null() Widget object.
  Widget_Dev() {}

  explicit Widget_Dev(PP_Resource resource);
  Widget_Dev(const Widget_Dev& other);

  // PPB_Widget methods:
  bool Paint(const Rect& rect, ImageData* image);
  bool HandleEvent(const InputEvent& event);
  bool GetLocation(Rect* location);
  void SetLocation(const Rect& location);
  void SetScale(float scale);
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_WIDGET_DEV_H_
