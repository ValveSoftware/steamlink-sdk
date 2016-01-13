// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "content/browser/device_sensors/data_fetcher_shared_memory.h"
#include "content/browser/device_sensors/device_inertial_sensor_service.h"
#include "content/common/device_sensors/device_motion_hardware_buffer.h"
#include "content/common/device_sensors/device_orientation_hardware_buffer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_javascript_dialog_manager.h"

namespace content {

namespace {

class FakeDataFetcher : public DataFetcherSharedMemory {
 public:
  FakeDataFetcher()
      : started_orientation_(false, false),
        stopped_orientation_(false, false),
        started_motion_(false, false),
        stopped_motion_(false, false),
        sensor_data_available_(true) {
  }
  virtual ~FakeDataFetcher() { }

  virtual bool Start(ConsumerType consumer_type, void* buffer) OVERRIDE {
    EXPECT_TRUE(buffer);

    switch (consumer_type) {
      case CONSUMER_TYPE_MOTION:
        {
          DeviceMotionHardwareBuffer* motion_buffer =
              static_cast<DeviceMotionHardwareBuffer*>(buffer);
          if (sensor_data_available_)
            UpdateMotion(motion_buffer);
          SetMotionBufferReady(motion_buffer);
          started_motion_.Signal();
        }
        break;
      case CONSUMER_TYPE_ORIENTATION:
        {
          DeviceOrientationHardwareBuffer* orientation_buffer =
              static_cast<DeviceOrientationHardwareBuffer*>(buffer);
          if (sensor_data_available_)
            UpdateOrientation(orientation_buffer);
          SetOrientationBufferReady(orientation_buffer);
          started_orientation_.Signal();
        }
        break;
      default:
        return false;
    }
    return true;
  }

  virtual bool Stop(ConsumerType consumer_type) OVERRIDE {
    switch (consumer_type) {
      case CONSUMER_TYPE_MOTION:
        stopped_motion_.Signal();
        break;
      case CONSUMER_TYPE_ORIENTATION:
        stopped_orientation_.Signal();
        break;
      default:
        return false;
    }
    return true;
  }

  virtual void Fetch(unsigned consumer_bitmask) OVERRIDE {
    FAIL() << "fetch should not be called";
  }

  virtual FetcherType GetType() const OVERRIDE {
    return FETCHER_TYPE_DEFAULT;
  }

  void SetSensorDataAvailable(bool available) {
    sensor_data_available_ = available;
  }

  void SetMotionBufferReady(DeviceMotionHardwareBuffer* buffer) {
    buffer->seqlock.WriteBegin();
    buffer->data.allAvailableSensorsAreActive = true;
    buffer->seqlock.WriteEnd();
  }

  void SetOrientationBufferReady(DeviceOrientationHardwareBuffer* buffer) {
    buffer->seqlock.WriteBegin();
    buffer->data.allAvailableSensorsAreActive = true;
    buffer->seqlock.WriteEnd();
  }

  void UpdateMotion(DeviceMotionHardwareBuffer* buffer) {
    buffer->seqlock.WriteBegin();
    buffer->data.accelerationX = 1;
    buffer->data.hasAccelerationX = true;
    buffer->data.accelerationY = 2;
    buffer->data.hasAccelerationY = true;
    buffer->data.accelerationZ = 3;
    buffer->data.hasAccelerationZ = true;

    buffer->data.accelerationIncludingGravityX = 4;
    buffer->data.hasAccelerationIncludingGravityX = true;
    buffer->data.accelerationIncludingGravityY = 5;
    buffer->data.hasAccelerationIncludingGravityY = true;
    buffer->data.accelerationIncludingGravityZ = 6;
    buffer->data.hasAccelerationIncludingGravityZ = true;

    buffer->data.rotationRateAlpha = 7;
    buffer->data.hasRotationRateAlpha = true;
    buffer->data.rotationRateBeta = 8;
    buffer->data.hasRotationRateBeta = true;
    buffer->data.rotationRateGamma = 9;
    buffer->data.hasRotationRateGamma = true;

    buffer->data.interval = 100;
    buffer->data.allAvailableSensorsAreActive = true;
    buffer->seqlock.WriteEnd();
  }

  void UpdateOrientation(DeviceOrientationHardwareBuffer* buffer) {
    buffer->seqlock.WriteBegin();
    buffer->data.alpha = 1;
    buffer->data.hasAlpha = true;
    buffer->data.beta = 2;
    buffer->data.hasBeta = true;
    buffer->data.gamma = 3;
    buffer->data.hasGamma = true;
    buffer->data.allAvailableSensorsAreActive = true;
    buffer->seqlock.WriteEnd();
  }

  base::WaitableEvent started_orientation_;
  base::WaitableEvent stopped_orientation_;
  base::WaitableEvent started_motion_;
  base::WaitableEvent stopped_motion_;
  bool sensor_data_available_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeDataFetcher);
};


class DeviceInertialSensorBrowserTest : public ContentBrowserTest  {
 public:
  DeviceInertialSensorBrowserTest()
      : fetcher_(NULL),
        io_loop_finished_event_(false, false) {
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&DeviceInertialSensorBrowserTest::SetUpOnIOThread, this));
    io_loop_finished_event_.Wait();
  }

  void SetUpOnIOThread() {
    fetcher_ = new FakeDataFetcher();
    DeviceInertialSensorService::GetInstance()->
        SetDataFetcherForTesting(fetcher_);
    io_loop_finished_event_.Signal();
  }

  void DelayAndQuit(base::TimeDelta delay) {
    base::PlatformThread::Sleep(delay);
    base::MessageLoop::current()->QuitWhenIdle();
  }

  void WaitForAlertDialogAndQuitAfterDelay(base::TimeDelta delay) {
    ShellJavaScriptDialogManager* dialog_manager=
        static_cast<ShellJavaScriptDialogManager*>(
            shell()->GetJavaScriptDialogManager());

    scoped_refptr<MessageLoopRunner> runner = new MessageLoopRunner();
    dialog_manager->set_dialog_request_callback(
        base::Bind(&DeviceInertialSensorBrowserTest::DelayAndQuit, this,
            delay));
    runner->Run();
  }

  FakeDataFetcher* fetcher_;

 private:
  base::WaitableEvent io_loop_finished_event_;
};


IN_PROC_BROWSER_TEST_F(DeviceInertialSensorBrowserTest, OrientationTest) {
  // The test page will register an event handler for orientation events,
  // expects to get an event with fake values, then removes the event
  // handler and navigates to #pass.
  GURL test_url = GetTestUrl(
      "device_orientation", "device_orientation_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);

  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
  fetcher_->started_orientation_.Wait();
  fetcher_->stopped_orientation_.Wait();
}

IN_PROC_BROWSER_TEST_F(DeviceInertialSensorBrowserTest, MotionTest) {
  // The test page will register an event handler for motion events,
  // expects to get an event with fake values, then removes the event
  // handler and navigates to #pass.
  GURL test_url = GetTestUrl(
      "device_orientation", "device_motion_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);

  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
  fetcher_->started_motion_.Wait();
  fetcher_->stopped_motion_.Wait();
}

// Flaking in the android try bot. See http://crbug.com/360578.
#if defined(OS_ANDROID)
#define MAYBE_OrientationNullTestWithAlert DISABLED_OrientationNullTestWithAlert
#else
#define MAYBE_OrientationNullTestWithAlert OrientationNullTestWithAlert
#endif
IN_PROC_BROWSER_TEST_F(DeviceInertialSensorBrowserTest,
    MAYBE_OrientationNullTestWithAlert) {
  // The test page will register an event handler for orientation events,
  // expects to get an event with null values. The test raises a modal alert
  // dialog with a delay to test that the one-off null-event still propagates
  // to window after the alert is dismissed and the callback is invoked which
  // navigates to #pass.
  fetcher_->SetSensorDataAvailable(false);
  TestNavigationObserver same_tab_observer(shell()->web_contents(), 2);

  GURL test_url = GetTestUrl(
      "device_orientation", "device_orientation_null_test_with_alert.html");
  shell()->LoadURL(test_url);

  // TODO(timvolodine): investigate if it is possible to test this without
  // delay, crbug.com/360044.
  WaitForAlertDialogAndQuitAfterDelay(base::TimeDelta::FromMilliseconds(1000));

  fetcher_->started_orientation_.Wait();
  fetcher_->stopped_orientation_.Wait();
  same_tab_observer.Wait();
  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
}

// Flaking in the android try bot. See http://crbug.com/360578.
#if defined(OS_ANDROID)
#define MAYBE_MotionNullTestWithAlert DISABLED_MotionNullTestWithAlert
#else
#define MAYBE_MotionNullTestWithAlert MotionNullTestWithAlert
#endif
IN_PROC_BROWSER_TEST_F(DeviceInertialSensorBrowserTest,
    MAYBE_MotionNullTestWithAlert) {
  // The test page will register an event handler for motion events,
  // expects to get an event with null values. The test raises a modal alert
  // dialog with a delay to test that the one-off null-event still propagates
  // to window after the alert is dismissed and the callback is invoked which
  // navigates to #pass.
  fetcher_->SetSensorDataAvailable(false);
  TestNavigationObserver same_tab_observer(shell()->web_contents(), 2);

  GURL test_url = GetTestUrl(
      "device_orientation", "device_motion_null_test_with_alert.html");
  shell()->LoadURL(test_url);

  // TODO(timvolodine): investigate if it is possible to test this without
  // delay, crbug.com/360044.
  WaitForAlertDialogAndQuitAfterDelay(base::TimeDelta::FromMilliseconds(1000));

  fetcher_->started_motion_.Wait();
  fetcher_->stopped_motion_.Wait();
  same_tab_observer.Wait();
  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
}

}  //  namespace

}  //  namespace content
