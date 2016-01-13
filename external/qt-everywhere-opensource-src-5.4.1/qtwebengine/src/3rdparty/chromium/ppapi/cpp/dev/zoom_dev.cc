// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/zoom_dev.h"

#include "ppapi/c/dev/ppb_zoom_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

static const char kPPPZoomInterface[] = PPP_ZOOM_DEV_INTERFACE;

void Zoom(PP_Instance instance,
          double factor,
          PP_Bool text_only) {
  void* object = Instance::GetPerInstanceObject(instance, kPPPZoomInterface);
  if (!object)
    return;
  static_cast<Zoom_Dev*>(object)->Zoom(factor, PP_ToBool(text_only));
}

const PPP_Zoom_Dev ppp_zoom = {
  &Zoom
};

template <> const char* interface_name<PPB_Zoom_Dev>() {
  return PPB_ZOOM_DEV_INTERFACE;
}

}  // namespace

Zoom_Dev::Zoom_Dev(Instance* instance) : associated_instance_(instance) {
  Module::Get()->AddPluginInterface(kPPPZoomInterface, &ppp_zoom);
  instance->AddPerInstanceObject(kPPPZoomInterface, this);
}

Zoom_Dev::~Zoom_Dev() {
  Instance::RemovePerInstanceObject(associated_instance_,
                                    kPPPZoomInterface, this);
}

void Zoom_Dev::ZoomChanged(double factor) {
  if (has_interface<PPB_Zoom_Dev>())
    get_interface<PPB_Zoom_Dev>()->ZoomChanged(
        associated_instance_.pp_instance(), factor);
}

void Zoom_Dev::ZoomLimitsChanged(double minimum_factor,
                                 double maximium_factor) {
  if (!has_interface<PPB_Zoom_Dev>())
    return;
  get_interface<PPB_Zoom_Dev>()->ZoomLimitsChanged(
      associated_instance_.pp_instance(), minimum_factor, maximium_factor);
}

}  // namespace pp
