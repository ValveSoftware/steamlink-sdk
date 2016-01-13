// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_PRIVATE_INPUT_EVENT_PRIVATE_H_
#define PPAPI_CPP_PRIVATE_INPUT_EVENT_PRIVATE_H_

#include "ppapi/c/private/ppb_input_event_private.h"
#include "ppapi/cpp/input_event.h"

namespace pp {

class InputEventPrivate : public InputEvent {
 public:
  InputEventPrivate();
  explicit InputEventPrivate(const InputEvent& event);

  bool TraceInputLatency(bool has_damage);

  /// See PPB_InputEventPrivate.StartTrackingLatency.
  static void StartTrackingLatency (const InstanceHandle& instance);

};

}

#endif   // PPAPI_CPP_PRIVATE_INPUT_EVENT_PRIVATE_H_
