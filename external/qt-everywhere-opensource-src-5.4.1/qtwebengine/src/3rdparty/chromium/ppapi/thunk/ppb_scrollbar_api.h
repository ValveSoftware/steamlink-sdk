// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_SCROLLBAR_API_H_
#define PPAPI_THUNK_PPB_SCROLLBAR_API_H_

#include "ppapi/c/dev/ppb_scrollbar_dev.h"

namespace ppapi {
namespace thunk {

class PPB_Scrollbar_API {
 public:
  virtual ~PPB_Scrollbar_API() {}

  virtual uint32_t GetThickness() = 0;
  virtual bool IsOverlay() = 0;
  virtual uint32_t GetValue() = 0;
  virtual void SetValue(uint32_t value) = 0;
  virtual void SetDocumentSize(uint32_t size) = 0;
  virtual void SetTickMarks(const PP_Rect* tick_marks, uint32_t count) = 0;
  virtual void ScrollBy(PP_ScrollBy_Dev unit, int32_t multiplier) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_SCROLLBAR_API_H_
