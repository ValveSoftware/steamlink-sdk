// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "content/browser/device_sensors/data_fetcher_shared_memory.h"
#include "content/browser/device_sensors/device_inertial_sensor_service.h"
#include "content/common/device_sensors/device_light_hardware_buffer.h"
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
      : started_orientation_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED),
        stopped_orientation_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                             base::WaitableEvent::InitialState::NOT_SIGNALED),
        started_motion_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                        base::WaitableEvent::InitialState::NOT_SIGNALED),
        stopped_motion_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                        base::WaitableEvent::InitialState::NOT_SIGNALED),
        started_light_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                       base::WaitableEvent::InitialState::NOT_SIGNALED),
        stopped_light_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                       base::WaitableEvent::InitialState::NOT_SIGNALED),
        sensor_data_available_(true) {}
  ~FakeDataFetcher() override {}

  bool Start(ConsumerType consumer_type, void* buffer) override {
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
      case CONSUMER_TYPE_LIGHT:
        {
          DeviceLightHardwareBuffer* light_buffer =
              static_cast<DeviceLightHardwareBuffer*>(buffer);
          UpdateLight(light_buffer,
                      sensor_data_available_
                          ? 100
                          : std::numeric_limits<double>::infinity());
          started_light_.Signal();
        }
        break;
      default:
        return false;
    }
    return true;
  }

  bool Stop(ConsumerType consumer_type) override {
    switch (consumer_type) {
      case CONSUMER_TYPE_MOTION:
        stopped_motion_.Signal();
        break;
      case CONSUMER_TYPE_ORIENTATION:
        stopped_orientation_.Signal();
        break;
      case CONSUMER_TYPE_LIGHT:
        stopped_light_.Signal();
        break;
      default:
        return false;
    }
    return true;
  }

  void Fetch(unsigned consumer_bitmask) override {
    FAIL() << "fetch should not be called";
  }

  FetcherType GetType() const override { return FETCHER_TYPE_DEFAULT; }

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

  void UpdateLight(DeviceLightHardwareBuffer* buffer, double lux) {
    buffer->seqlock.WriteBegin();
    buffer->data.value = lux;
    buffer->seqlock.WriteEnd();
  }

  base::WaitableEvent started_orientation_;
  base::WaitableEvent stopped_orientation_;
  base::WaitableEvent started_motion_;
  base::WaitableEvent stopped_motion_;
  base::WaitableEvent started_light_;
  base::WaitableEvent stopped_light_;
  bool sensor_data_available_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeDataFetcher);
};


class DeviceInertialSensorBrowserTest : public ContentBrowserTest  {
 public:
  DeviceInertialSensorBrowserTest()
      : fetcher_(nullptr),
        io_loop_finished_event_(
            base::WaitableEvent::ResetPolicy::AUTOMATIC,
            base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  void SetUpOnMainThread() override {
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
    ShellJavaScriptDialogManager* dialog_manager =
        static_cast<ShellJavaScriptDialogManager*>(
            shell()->GetJavaScriptDialogManager(shell()->web_contents()));

    scoped_refptr<MessageLoopRunner> runner = new MessageLoopRunner();
    dialog_manager->set_dialog_request_callback(
        base::Bind(&DeviceInertialSensorBrowserTest::DelayAndQuit, this,
            delay));
    runner->Run();
  }

  void EnableExperimentalFeatures() {
    // TODO(riju): remove when the DeviceLight feature goes stable.
    base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
    if (!cmd_line->HasSwitch(switches::kEnableExperimentalWebPlatformFeatures))
      cmd_line->AppendSwitch(switches::kEnableExperimentalWebPlatformFeatures);
  }

  FakeDataFetcher* fetcher_;

 private:
  base::WaitableEvent io_loop_finished_event_;
};

IN_PROC_BROWSER_TEST_F(DeviceInertialSensorBrowserTest, OrientationTest) {
  // The test page will register an event handler for orientation events,
  // expects to get an event with fake values, then removes the event
  // handler and navigates to #pass.
  GURL test_url = GetTestUrl("device_sensors", "device_orientation_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);

  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
  fetcher_->started_orientation_.Wait();
  fetcher_->stopped_orientation_.Wait();
}

IN_PROC_BROWSER_TEST_F(DeviceInertialSensorBrowserTest, LightTest) {
  // The test page will register an event handler for light events,
  // expects to get an event with fake values, then removes the event
  // handler and navigates to #pass.
  EnableExperimentalFeatures();
  GURL test_url = GetTestUrl("device_sensors", "device_light_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);

  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
  fetcher_->started_light_.Wait();
  fetcher_->stopped_light_.Wait();
}

IN_PROC_BROWSER_TEST_F(DeviceInertialSensorBrowserTest, MotionTest) {
  // The test page will register an event handler for motion events,
  // expects to get an event with fake values, then removes the event
  // handler and navigates to #pass.
  GURL test_url = GetTestUrl("device_sensors", "device_motion_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);

  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
  fetcher_->started_motion_.Wait();
  fetcher_->stopped_motion_.Wait();
}

IN_PROC_BROWSER_TEST_F(DeviceInertialSensorBrowserTest,
    LightOneOffInfintyTest) {
  // The test page registers an event handler for light events and expects
  // to get an event with value equal to infinity, because no sensor data can
  // be provided.
  EnableExperimentalFeatures();
  fetcher_->SetSensorDataAvailable(false);
  GURL test_url = GetTestUrl("device_sensors",
                             "device_light_infinity_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);

  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
  fetcher_->started_light_.Wait();
  fetcher_->stopped_light_.Wait();
}

IN_PROC_BROWSER_TEST_F(DeviceInertialSensorBrowserTest, OrientationNullTest) {
  // The test page registers an event handler for orientation events and
  // expects to get an event with null values, because no sensor data can be
  // provided.
  fetcher_->SetSensorDataAvailable(false);
  GURL test_url = GetTestUrl("device_sensors",
                             "device_orientation_null_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);

  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
  fetcher_->started_orientation_.Wait();
  fetcher_->stopped_orientation_.Wait();
}

IN_PROC_BROWSER_TEST_F(DeviceInertialSensorBrowserTest, MotionNullTest) {
  // The test page registers an event handler for motion events and
  // expects to get an event with null values, because no sensor data can be
  // provided.
  fetcher_->SetSensorDataAvailable(false);
  GURL test_url = GetTestUrl("device_sensors",
                             "device_motion_null_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);

  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
  fetcher_->started_motion_.Wait();
  fetcher_->stopped_motion_.Wait();
}

IN_PROC_BROWSER_TEST_F(DeviceInertialSensorBrowserTest,
                       DISABLED_NullTestWithAlert) {
  // The test page registers an event handlers for motion/orientation events
  // and expects to get events with null values. The test raises a modal alert
  // dialog with a delay to test that the one-off null-events still propagate
  // to window after the alert is dismissed and the callbacks are invoked which
  // eventually navigate to #pass.
  fetcher_->SetSensorDataAvailable(false);
  TestNavigationObserver same_tab_observer(shell()->web_contents(), 2);

  GURL test_url = GetTestUrl("device_sensors",
                             "device_sensors_null_test_with_alert.html");
  shell()->LoadURL(test_url);

  // TODO(timvolodine): investigate if it is possible to test this without
  // delay, crbug.com/360044.
  WaitForAlertDialogAndQuitAfterDelay(base::TimeDelta::FromMilliseconds(500));

  fetcher_->started_motion_.Wait();
  fetcher_->stopped_motion_.Wait();
  fetcher_->started_orientation_.Wait();
  fetcher_->stopped_orientation_.Wait();
  same_tab_observer.Wait();
  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
}

}  //  namespace

}  //  namespace content
