// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/linux/video_capture_device_chromeos.h"

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop_proxy.h"
#include "ui/gfx/display.h"
#include "ui/gfx/display_observer.h"
#include "ui/gfx/screen.h"

namespace media {

// This is a delegate class used to transfer Display change events from the UI
// thread to the media thread.
class VideoCaptureDeviceChromeOS::ScreenObserverDelegate
    : public gfx::DisplayObserver,
      public base::RefCountedThreadSafe<ScreenObserverDelegate> {
 public:
  ScreenObserverDelegate(
      VideoCaptureDeviceChromeOS* capture_device,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
      : capture_device_(capture_device),
        ui_task_runner_(ui_task_runner),
        capture_task_runner_(base::MessageLoopProxy::current()) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ScreenObserverDelegate::AddObserverOnUIThread, this));
  }

  void RemoveObserver() {
    DCHECK(capture_task_runner_->BelongsToCurrentThread());
    capture_device_ = NULL;
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ScreenObserverDelegate::RemoveObserverOnUIThread, this));
  }

 private:
  friend class base::RefCountedThreadSafe<ScreenObserverDelegate>;

  virtual ~ScreenObserverDelegate() {
    DCHECK(!capture_device_);
  }

  virtual void OnDisplayAdded(const gfx::Display& /*new_display*/) OVERRIDE {}
  virtual void OnDisplayRemoved(const gfx::Display& /*old_display*/) OVERRIDE {}
  virtual void OnDisplayMetricsChanged(const gfx::Display& display,
                                       uint32_t metrics) OVERRIDE {
    DCHECK(ui_task_runner_->BelongsToCurrentThread());
    if (!(metrics & DISPLAY_METRIC_ROTATION))
      return;
    SendDisplayRotation(display);
  }

  void AddObserverOnUIThread() {
    DCHECK(ui_task_runner_->BelongsToCurrentThread());
    gfx::Screen* screen =
        gfx::Screen::GetScreenByType(gfx::SCREEN_TYPE_ALTERNATE);
    if (screen) {
      screen->AddObserver(this);
      SendDisplayRotation(screen->GetPrimaryDisplay());
    }
  }

  void RemoveObserverOnUIThread() {
    DCHECK(ui_task_runner_->BelongsToCurrentThread());
    gfx::Screen* screen =
        gfx::Screen::GetScreenByType(gfx::SCREEN_TYPE_ALTERNATE);
    if (screen)
      screen->RemoveObserver(this);
  }

  // Post the screen rotation change from the UI thread to capture thread
  void SendDisplayRotation(const gfx::Display& display) {
    DCHECK(ui_task_runner_->BelongsToCurrentThread());
    capture_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ScreenObserverDelegate::SendDisplayRotationOnCaptureThread,
                   this, display));
  }

  void SendDisplayRotationOnCaptureThread(const gfx::Display& display) {
    DCHECK(capture_task_runner_->BelongsToCurrentThread());
    if (capture_device_)
      capture_device_->SetDisplayRotation(display);
  }

  VideoCaptureDeviceChromeOS* capture_device_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(ScreenObserverDelegate);
};


VideoCaptureDeviceChromeOS::VideoCaptureDeviceChromeOS(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    const Name& device_name)
    : VideoCaptureDeviceLinux(device_name),
      screen_observer_delegate_(new ScreenObserverDelegate(this,
                                                           ui_task_runner)) {
}

VideoCaptureDeviceChromeOS::~VideoCaptureDeviceChromeOS() {
  screen_observer_delegate_->RemoveObserver();
}

void VideoCaptureDeviceChromeOS::SetDisplayRotation(
    const gfx::Display& display) {
  if (display.IsInternal())
    SetRotation(display.rotation() * 90);
}

}  // namespace media
