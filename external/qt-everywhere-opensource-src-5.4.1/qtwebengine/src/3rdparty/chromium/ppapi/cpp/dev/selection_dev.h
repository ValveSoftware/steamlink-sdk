// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_DEV_SELECTION_DEV_H_
#define PPAPI_CPP_DEV_SELECTION_DEV_H_

#include "ppapi/c/dev/ppp_selection_dev.h"
#include "ppapi/cpp/instance_handle.h"

namespace pp {

class Var;

// This class allows you to associate the PPP_Selection_Dev C-based interface
// with an object. It registers as the global handler for handling the
// PPP_Selection_Dev interface that the browser calls.
//
// You would typically use this either via inheritance on your instance:
//   class MyInstance : public pp::Instance, public pp::Selection_Dev {
//     class MyInstance() : pp::Selection_Dev(this) {
//     }
//     ...
//   };
//
// or by composition:
//   class MySelection : public pp::Selection_Dev {
//     ...
//   };
//
//   class MyInstance : public pp::Instance {
//     MyInstance() : selection_(this) {
//     }
//
//     MySelection selection_;
//   };
class Selection_Dev {
 public:
  explicit Selection_Dev(Instance* instance);
  virtual ~Selection_Dev();

  // PPP_Selection_Dev functions exposed as virtual functions for you to
  // override.
  virtual Var GetSelectedText(bool html) = 0;

 private:
  InstanceHandle associated_instance_;
};

}  // namespace pp

#endif  // PPAPI_CPP_DEV_SELECTION_DEV_H_
