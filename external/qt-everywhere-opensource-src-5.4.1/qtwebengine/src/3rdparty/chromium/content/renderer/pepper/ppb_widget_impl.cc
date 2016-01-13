// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/ppb_widget_impl.h"

#include "content/renderer/pepper/host_globals.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/ppb_image_data_impl.h"
#include "content/renderer/pepper/plugin_module.h"
#include "ppapi/c/dev/ppp_widget_dev.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_input_event_api.h"
#include "ppapi/thunk/ppb_widget_api.h"

using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_ImageData_API;
using ppapi::thunk::PPB_InputEvent_API;
using ppapi::thunk::PPB_Widget_API;

namespace content {

PPB_Widget_Impl::PPB_Widget_Impl(PP_Instance instance)
    : Resource(ppapi::OBJECT_IS_IMPL, instance), scale_(1.0f) {
  memset(&location_, 0, sizeof(location_));
}

PPB_Widget_Impl::~PPB_Widget_Impl() {}

PPB_Widget_API* PPB_Widget_Impl::AsPPB_Widget_API() { return this; }

PP_Bool PPB_Widget_Impl::Paint(const PP_Rect* rect, PP_Resource image_id) {
  EnterResourceNoLock<PPB_ImageData_API> enter(image_id, true);
  if (enter.failed())
    return PP_FALSE;
  return PaintInternal(
      gfx::Rect(
          rect->point.x, rect->point.y, rect->size.width, rect->size.height),
      static_cast<PPB_ImageData_Impl*>(enter.object()));
}

PP_Bool PPB_Widget_Impl::HandleEvent(PP_Resource pp_input_event) {
  EnterResourceNoLock<PPB_InputEvent_API> enter(pp_input_event, true);
  if (enter.failed())
    return PP_FALSE;
  return HandleEventInternal(enter.object()->GetInputEventData());
}

PP_Bool PPB_Widget_Impl::GetLocation(PP_Rect* location) {
  *location = location_;
  return PP_TRUE;
}

void PPB_Widget_Impl::SetLocation(const PP_Rect* location) {
  location_ = *location;
  SetLocationInternal(location);
}

void PPB_Widget_Impl::SetScale(float scale) { scale_ = scale; }

void PPB_Widget_Impl::Invalidate(const PP_Rect* dirty) {
  PepperPluginInstanceImpl* plugin_instance =
      HostGlobals::Get()->GetInstance(pp_instance());
  if (!plugin_instance)
    return;
  const PPP_Widget_Dev* widget = static_cast<const PPP_Widget_Dev*>(
      plugin_instance->module()->GetPluginInterface(PPP_WIDGET_DEV_INTERFACE));
  if (!widget)
    return;
  widget->Invalidate(pp_instance(), pp_resource(), dirty);
}

}  // namespace content
