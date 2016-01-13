// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DEVICE_SENSORS_DEVICE_SENSOR_EVENT_PUMP_H_
#define CONTENT_RENDERER_DEVICE_SENSORS_DEVICE_SENSOR_EVENT_PUMP_H_

#include "base/memory/shared_memory.h"
#include "base/timer/timer.h"
#include "content/public/renderer/render_process_observer.h"

namespace content {
class RenderThread;

class CONTENT_EXPORT DeviceSensorEventPump : public RenderProcessObserver {
 public:
  // Default delay between subsequent firing of events.
  static const int kDefaultPumpDelayMillis;

  int GetDelayMillis() const;

  void Attach(RenderThread* thread);
  virtual bool OnControlMessageReceived(const IPC::Message& message) = 0;

 protected:
  // Constructor for a pump with default delay.
  DeviceSensorEventPump();

  // Constructor for a pump with a given delay.
  explicit DeviceSensorEventPump(int pump_delay_millis);
  virtual ~DeviceSensorEventPump();

  // The pump is a tri-state automaton with allowed transitions as follows:
  // STOPPED -> PENDING_START
  // PENDING_START -> RUNNING
  // PENDING_START -> STOPPED
  // RUNNING -> STOPPED
  enum PumpState {
      STOPPED,
      RUNNING,
      PENDING_START
  };

  bool RequestStart();
  void OnDidStart(base::SharedMemoryHandle handle);
  bool Stop();

  virtual void FireEvent() = 0;
  virtual bool InitializeReader(base::SharedMemoryHandle handle) = 0;
  virtual bool SendStartMessage() = 0;
  virtual bool SendStopMessage() = 0;

  int pump_delay_millis_;
  PumpState state_;
  base::RepeatingTimer<DeviceSensorEventPump> timer_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_DEVICE_SENSORS_DEVICE_SENSOR_EVENT_PUMP_H_
