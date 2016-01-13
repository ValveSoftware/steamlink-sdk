// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_WIDGET_API_H_
#define PPAPI_THUNK_PPB_WIDGET_API_H_

#include "ppapi/c/dev/ppb_widget_dev.h"

namespace ppapi {
namespace thunk {

class PPB_Widget_API {
 public:
  virtual ~PPB_Widget_API() {}

  virtual PP_Bool Paint(const PP_Rect* rect, PP_Resource image_id) = 0;
  virtual PP_Bool HandleEvent(PP_Resource pp_input_event) = 0;
  virtual PP_Bool GetLocation(PP_Rect* location) = 0;
  virtual void SetLocation(const PP_Rect* location) = 0;
  virtual void SetScale(float scale) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_WIDGET_API_H_
