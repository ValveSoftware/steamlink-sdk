// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_INPUT_EVENT_HANDLER_H_
#define COMPONENTS_MUS_PUBLIC_CPP_INPUT_EVENT_HANDLER_H_

#include <memory>

#include "base/callback_forward.h"

namespace ui {
class Event;
}

namespace mus {

class Window;

namespace mojom {
enum class EventResult;
}

// Responsible for processing input events for mus::Window.
class InputEventHandler {
 public:
  // The event handler can asynchronously ack the event by taking ownership of
  // the |ack_callback|. The callback takes an EventResult indicating if the
  // handler has consumed the event. If the handler does not take ownership of
  // the callback, then WindowTreeClient will ack the event as not consumed.
  virtual void OnWindowInputEvent(
      Window* target,
      const ui::Event& event,
      std::unique_ptr<base::Callback<void(mojom::EventResult)>>*
          ack_callback) = 0;

 protected:
  virtual ~InputEventHandler() {}
};

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_INPUT_EVENT_HANDLER_H_
