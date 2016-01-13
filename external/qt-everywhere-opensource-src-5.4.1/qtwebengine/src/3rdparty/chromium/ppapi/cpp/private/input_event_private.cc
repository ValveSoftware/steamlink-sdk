// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/private/input_event_private.h"

#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_InputEvent_Private>() {
  return PPB_INPUTEVENT_PRIVATE_INTERFACE;
}

}  // namespace

InputEventPrivate::InputEventPrivate() : InputEvent() {
}

InputEventPrivate::InputEventPrivate(const InputEvent& event) : InputEvent() {
  if (!has_interface<PPB_InputEvent_Private_0_1>())
    return;
  Module::Get()->core()->AddRefResource(event.pp_resource());
  PassRefFromConstructor(event.pp_resource());
}

bool InputEventPrivate::TraceInputLatency(bool has_damage) {
  if (!has_interface<PPB_InputEvent_Private_0_1>())
    return false;
  return PP_ToBool(
      get_interface<PPB_InputEvent_Private_0_1>()->TraceInputLatency(
          pp_resource(), PP_FromBool(has_damage)));
}

void InputEventPrivate::StartTrackingLatency(const InstanceHandle& instance) {
  if (!has_interface<PPB_InputEvent_Private>())
    return;
  return get_interface<PPB_InputEvent_Private>()->StartTrackingLatency(
      instance.pp_instance());
}

}  // namespace pp
