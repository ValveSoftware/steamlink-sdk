// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DEVICE_SENSORS_DEVICE_SENSOR_EVENT_PUMP_H_
#define CONTENT_RENDERER_DEVICE_SENSORS_DEVICE_SENSOR_EVENT_PUMP_H_

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/shared_memory.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/public/renderer/platform_event_observer.h"
#include "content/renderer/render_thread_impl.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace content {

template <typename Base, typename MojoInterface>
class CONTENT_EXPORT DeviceSensorMojoClientMixin : public Base {
 public:
  template <typename... Args>
  explicit DeviceSensorMojoClientMixin(Args&&... args)
      : Base(std::forward<Args>(args)...) {
    mojo::InterfaceRequest<MojoInterface> request =
        mojo::GetProxy(&mojo_interface_);

    // When running layout tests, those observers should not listen to the
    // actual hardware changes. In order to make that happen, don't connect
    // the other end of the mojo pipe to anything.
    //
    // TODO(sammc): Remove this when JS layout test support for shared buffers
    // is ready and the layout tests are converted to use that for mocking.
    if (RenderThreadImpl::current() &&
        !RenderThreadImpl::current()->layout_test_mode()) {
      RenderThread::Get()->GetRemoteInterfaces()->GetInterface(
          std::move(request));
    }
  }

  void SendStartMessage() override {
    mojo_interface_->StartPolling(
        base::Bind(&DeviceSensorMojoClientMixin<Base, MojoInterface>::DidStart,
                   base::Unretained(this)));
  }
  void SendStopMessage() override { mojo_interface_->StopPolling(); }

 protected:
  void DidStart(mojo::ScopedSharedBufferHandle buffer_handle) {
    Base::DidStart(std::move(buffer_handle));
  }

 private:
  mojo::InterfacePtr<MojoInterface> mojo_interface_;
};

template <typename ListenerType>
class CONTENT_EXPORT DeviceSensorEventPump
    : NON_EXPORTED_BASE(public PlatformEventObserver<ListenerType>) {
 public:
  // Default rate for firing events.
  static const int kDefaultPumpFrequencyHz = 60;
  static const int kDefaultPumpDelayMicroseconds =
      base::Time::kMicrosecondsPerSecond / kDefaultPumpFrequencyHz;

  // PlatformEventObserver
  void Start(blink::WebPlatformEventListener* listener) override {
    DVLOG(2) << "requested start";

    if (state_ != STOPPED)
      return;

    DCHECK(!timer_.IsRunning());

    PlatformEventObserver<ListenerType>::Start(listener);
    state_ = PENDING_START;
  }

  void Stop() override {
    DVLOG(2) << "stop";

    if (state_ == STOPPED)
      return;

    DCHECK((state_ == PENDING_START && !timer_.IsRunning()) ||
        (state_ == RUNNING && timer_.IsRunning()));

    if (timer_.IsRunning())
      timer_.Stop();
    PlatformEventObserver<ListenerType>::Stop();
    state_ = STOPPED;
  }

 protected:
  explicit DeviceSensorEventPump(RenderThread* thread)
      : PlatformEventObserver<ListenerType>(thread),
        pump_delay_microseconds_(kDefaultPumpDelayMicroseconds),
        state_(STOPPED) {}

  ~DeviceSensorEventPump() override {
    PlatformEventObserver<ListenerType>::StopIfObserving();
  }

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

  void DidStart(mojo::ScopedSharedBufferHandle buffer_handle) {
    DVLOG(2) << "did start sensor event pump";

    if (state_ != PENDING_START)
      return;

    DCHECK(!timer_.IsRunning());

    base::SharedMemoryHandle handle;
    MojoResult result = mojo::UnwrapSharedMemoryHandle(
        std::move(buffer_handle), &handle, nullptr, nullptr);
    DCHECK_EQ(MOJO_RESULT_OK, result);

    if (InitializeReader(handle)) {
      timer_.Start(FROM_HERE,
                   base::TimeDelta::FromMicroseconds(pump_delay_microseconds_),
                   this,
                   &DeviceSensorEventPump::FireEvent);
      state_ = RUNNING;
    }
  }

  virtual void FireEvent() = 0;
  virtual bool InitializeReader(base::SharedMemoryHandle handle) = 0;

  int pump_delay_microseconds_;
  PumpState state_;
  base::RepeatingTimer timer_;

  DISALLOW_COPY_AND_ASSIGN(DeviceSensorEventPump);
};

}  // namespace content

#endif  // CONTENT_RENDERER_DEVICE_SENSORS_DEVICE_SENSOR_EVENT_PUMP_H_
