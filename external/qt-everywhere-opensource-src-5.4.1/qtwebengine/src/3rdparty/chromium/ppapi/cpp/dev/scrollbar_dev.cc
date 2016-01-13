// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "ppapi/cpp/dev/scrollbar_dev.h"

#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/rect.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Scrollbar_Dev>() {
  return PPB_SCROLLBAR_DEV_INTERFACE;
}

}  // namespace

Scrollbar_Dev::Scrollbar_Dev(PP_Resource resource) : Widget_Dev(resource) {
}

Scrollbar_Dev::Scrollbar_Dev(const InstanceHandle& instance, bool vertical) {
  if (!has_interface<PPB_Scrollbar_Dev>())
    return;
  PassRefFromConstructor(get_interface<PPB_Scrollbar_Dev>()->Create(
      instance.pp_instance(), PP_FromBool(vertical)));
}

Scrollbar_Dev::Scrollbar_Dev(const Scrollbar_Dev& other)
    : Widget_Dev(other) {
}

uint32_t Scrollbar_Dev::GetThickness() {
  if (!has_interface<PPB_Scrollbar_Dev>())
    return 0;
  return get_interface<PPB_Scrollbar_Dev>()->GetThickness(pp_resource());
}

bool Scrollbar_Dev::IsOverlay() {
  if (!has_interface<PPB_Scrollbar_Dev>())
    return false;
  return
      PP_ToBool(get_interface<PPB_Scrollbar_Dev>()->IsOverlay(pp_resource()));
}

uint32_t Scrollbar_Dev::GetValue() {
  if (!has_interface<PPB_Scrollbar_Dev>())
    return 0;
  return get_interface<PPB_Scrollbar_Dev>()->GetValue(pp_resource());
}

void Scrollbar_Dev::SetValue(uint32_t value) {
  if (has_interface<PPB_Scrollbar_Dev>())
    get_interface<PPB_Scrollbar_Dev>()->SetValue(pp_resource(), value);
}

void Scrollbar_Dev::SetDocumentSize(uint32_t size) {
  if (has_interface<PPB_Scrollbar_Dev>())
    get_interface<PPB_Scrollbar_Dev>()->SetDocumentSize(pp_resource(), size);
}

void Scrollbar_Dev::SetTickMarks(const Rect* tick_marks, uint32_t count) {
  if (!has_interface<PPB_Scrollbar_Dev>())
    return;

  std::vector<PP_Rect> temp;
  temp.resize(count);
  for (uint32_t i = 0; i < count; ++i)
    temp[i] = tick_marks[i];

  get_interface<PPB_Scrollbar_Dev>()->SetTickMarks(
      pp_resource(), count ? &temp[0] : NULL, count);
}

void Scrollbar_Dev::ScrollBy(PP_ScrollBy_Dev unit, int32_t multiplier) {
  if (has_interface<PPB_Scrollbar_Dev>())
    get_interface<PPB_Scrollbar_Dev>()->ScrollBy(pp_resource(),
                                                 unit,
                                                 multiplier);
}

}  // namespace pp
