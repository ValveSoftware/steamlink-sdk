// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_ZOOM_DEV_H_
#define PPAPI_CPP_DEV_ZOOM_DEV_H_

#include <string>

#include "ppapi/c/dev/ppp_zoom_dev.h"
#include "ppapi/cpp/instance_handle.h"

namespace pp {

class Instance;

// This class allows you to associate the PPP_Zoom_Dev and PPB_Zoom_Dev C-based
// interfaces with an object. It associates itself with the given instance, and
// registers as the global handler for handling the PPP_Zoom_Dev interface that
// the browser calls.
//
// You would typically use this either via inheritance on your instance:
//   class MyInstance : public pp::Instance, public pp::Zoom_Dev {
//     class MyInstance() : pp::Zoom_Dev(this) {
//     }
//     ...
//   };
//
// or by composition:
//   class MyZoom : public pp::Zoom_Dev {
//     ...
//   };
//
//   class MyInstance : public pp::Instance {
//     MyInstance() : zoom_(this) {
//     }
//
//     MyZoom zoom_;
//   };
class Zoom_Dev {
 public:
  explicit Zoom_Dev(Instance* instance);
  virtual ~Zoom_Dev();

  // PPP_Zoom_Dev functions exposed as virtual functions for you to
  // override.
  virtual void Zoom(double factor, bool text_only) = 0;

  // PPB_Zoom_Def functions for you to call to report new zoom factor.
  void ZoomChanged(double factor);
  void ZoomLimitsChanged(double minimum_factor, double maximium_factor);

 private:
  InstanceHandle associated_instance_;
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_ZOOM_DEV_H_
