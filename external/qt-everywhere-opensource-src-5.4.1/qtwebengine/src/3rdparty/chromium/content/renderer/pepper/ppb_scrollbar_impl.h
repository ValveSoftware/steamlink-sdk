// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PPB_SCROLLBAR_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PPB_SCROLLBAR_IMPL_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/renderer/pepper/ppb_widget_impl.h"
#include "ppapi/thunk/ppb_scrollbar_api.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/web/WebPluginScrollbarClient.h"
#include "ui/gfx/rect.h"

namespace content {

class PPB_Scrollbar_Impl : public PPB_Widget_Impl,
                           public ppapi::thunk::PPB_Scrollbar_API,
                           public blink::WebPluginScrollbarClient {
 public:
  static PP_Resource Create(PP_Instance instance, bool vertical);

  // Resource overrides.
  virtual PPB_Scrollbar_API* AsPPB_Scrollbar_API() OVERRIDE;
  virtual void InstanceWasDeleted() OVERRIDE;

  // PPB_Scrollbar_API implementation.
  virtual uint32_t GetThickness() OVERRIDE;
  virtual bool IsOverlay() OVERRIDE;
  virtual uint32_t GetValue() OVERRIDE;
  virtual void SetValue(uint32_t value) OVERRIDE;
  virtual void SetDocumentSize(uint32_t size) OVERRIDE;
  virtual void SetTickMarks(const PP_Rect* tick_marks, uint32_t count) OVERRIDE;
  virtual void ScrollBy(PP_ScrollBy_Dev unit, int32_t multiplier) OVERRIDE;

 private:
  virtual ~PPB_Scrollbar_Impl();

  explicit PPB_Scrollbar_Impl(PP_Instance instance);
  void Init(bool vertical);

  // PPB_Widget private implementation.
  virtual PP_Bool PaintInternal(const gfx::Rect& rect,
                                PPB_ImageData_Impl* image) OVERRIDE;
  virtual PP_Bool HandleEventInternal(const ppapi::InputEventData& data)
      OVERRIDE;
  virtual void SetLocationInternal(const PP_Rect* location) OVERRIDE;

  // blink::WebPluginScrollbarClient implementation.
  virtual void valueChanged(blink::WebPluginScrollbar* scrollbar) OVERRIDE;
  virtual void overlayChanged(blink::WebPluginScrollbar* scrollbar) OVERRIDE;
  virtual void invalidateScrollbarRect(blink::WebPluginScrollbar* scrollbar,
                                       const blink::WebRect& rect) OVERRIDE;
  virtual void getTickmarks(blink::WebPluginScrollbar* scrollbar,
                            blink::WebVector<blink::WebRect>* tick_marks) const
      OVERRIDE;

  void NotifyInvalidate();

  gfx::Rect dirty_;
  std::vector<blink::WebRect> tickmarks_;
  scoped_ptr<blink::WebPluginScrollbar> scrollbar_;

  // Used so that the post task for Invalidate doesn't keep an extra reference.
  base::WeakPtrFactory<PPB_Scrollbar_Impl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Scrollbar_Impl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PPB_SCROLLBAR_IMPL_H_
