// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_BATTERY_STATUS_BATTERY_STATUS_DISPATCHER_H_
#define CONTENT_RENDERER_BATTERY_STATUS_BATTERY_STATUS_DISPATCHER_H_

#include "content/public/renderer/render_process_observer.h"

namespace blink {
class WebBatteryStatus;
class WebBatteryStatusListener;
}

namespace content {
class RenderThread;

class CONTENT_EXPORT BatteryStatusDispatcher : public RenderProcessObserver {
 public:
  explicit BatteryStatusDispatcher(RenderThread* thread);
  virtual ~BatteryStatusDispatcher();

  // RenderProcessObserver method.
  virtual bool OnControlMessageReceived(const IPC::Message& message) OVERRIDE;

  // Sets the listener to receive battery status updates. Returns true if the
  // registration was successful.
  bool SetListener(blink::WebBatteryStatusListener* listener);

 protected:
  virtual bool Start();
  virtual bool Stop();

 private:
  void OnDidChange(const blink::WebBatteryStatus& status);

  blink::WebBatteryStatusListener* listener_;

  DISALLOW_COPY_AND_ASSIGN(BatteryStatusDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_BATTERY_STATUS_BATTERY_STATUS_DISPATCHER_H_
