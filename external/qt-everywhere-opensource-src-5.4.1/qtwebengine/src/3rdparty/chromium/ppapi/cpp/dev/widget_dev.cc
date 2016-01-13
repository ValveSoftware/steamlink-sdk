// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/widget_dev.h"

#include "ppapi/c/dev/ppb_widget_dev.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Widget_Dev_0_3>() {
  return PPB_WIDGET_DEV_INTERFACE_0_3;
}

template <> const char* interface_name<PPB_Widget_Dev_0_4>() {
  return PPB_WIDGET_DEV_INTERFACE_0_4;
}

}  // namespace

Widget_Dev::Widget_Dev(PP_Resource resource) : Resource(resource) {
}

Widget_Dev::Widget_Dev(const Widget_Dev& other) : Resource(other) {
}

bool Widget_Dev::Paint(const Rect& rect, ImageData* image) {
  if (has_interface<PPB_Widget_Dev_0_4>()) {
    return PP_ToBool(get_interface<PPB_Widget_Dev_0_4>()->Paint(
        pp_resource(), &rect.pp_rect(), image->pp_resource()));
  } else if (has_interface<PPB_Widget_Dev_0_3>()) {
    return PP_ToBool(get_interface<PPB_Widget_Dev_0_3>()->Paint(
        pp_resource(), &rect.pp_rect(), image->pp_resource()));
  }
  return false;
}

bool Widget_Dev::HandleEvent(const InputEvent& event) {
  if (has_interface<PPB_Widget_Dev_0_4>()) {
    return PP_ToBool(get_interface<PPB_Widget_Dev_0_4>()->HandleEvent(
        pp_resource(), event.pp_resource()));
  } else if (has_interface<PPB_Widget_Dev_0_3>()) {
    return PP_ToBool(get_interface<PPB_Widget_Dev_0_3>()->HandleEvent(
        pp_resource(), event.pp_resource()));
  }
  return false;
}

bool Widget_Dev::GetLocation(Rect* location) {
  if (has_interface<PPB_Widget_Dev_0_4>()) {
    return PP_ToBool(get_interface<PPB_Widget_Dev_0_4>()->GetLocation(
        pp_resource(), &location->pp_rect()));
  } else if (has_interface<PPB_Widget_Dev_0_3>()) {
    return PP_ToBool(get_interface<PPB_Widget_Dev_0_3>()->GetLocation(
        pp_resource(), &location->pp_rect()));
  }
  return false;
}

void Widget_Dev::SetLocation(const Rect& location) {
  if (has_interface<PPB_Widget_Dev_0_4>()) {
    get_interface<PPB_Widget_Dev_0_4>()->SetLocation(pp_resource(),
                                                     &location.pp_rect());
  } else if (has_interface<PPB_Widget_Dev_0_3>()) {
    get_interface<PPB_Widget_Dev_0_3>()->SetLocation(pp_resource(),
                                                     &location.pp_rect());
  }
}

void Widget_Dev::SetScale(float scale) {
  if (has_interface<PPB_Widget_Dev_0_4>())
    get_interface<PPB_Widget_Dev_0_4>()->SetScale(pp_resource(), scale);
}

}  // namespace pp
