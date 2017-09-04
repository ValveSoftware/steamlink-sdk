// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "content/browser/device_sensors/data_fetcher_shared_memory.h"
#include "content/browser/device_sensors/device_sensor_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_javascript_dialog_manager.h"
#include "device/sensors/public/cpp/device_light_hardware_buffer.h"
#include "device/sensors/public/cpp/device_motion_hardware_buffer.h"
#include "device/sensors/public/cpp/device_orientation_hardware_buffer.h"

namespace content {

namespace {

class FakeDataFetcher : public DataFetcherSharedMemory {
 public:
  FakeDataFetcher() : sensor_data_available_(true) {}
  ~FakeDataFetcher() override {}

  void SetMotionStartedCallback(base::Closure motion_started_callback) {
    motion_started_callback_ = motion_started_callback;
  }

  void SetMotionStoppedCallback(base::Closure motion_stopped_callback) {
    motion_stopped_callback_ = motion_stopped_callback;
  }

  void SetLightStartedCallback(base::Closure light_started_callback) {
    light_started_callback_ = light_started_callback;
  }

  void SetLightStoppedCallback(base::Closure light_stopped_callback) {
    light_stopped_callback_ = light_stopped_callback;
  }

  void SetOrientationStartedCallback(
      base::Closure orientation_started_callback) {
    orientation_started_callback_ = orientation_started_callback;
  }

  void SetOrientationStoppedCallback(
      base::Closure orientation_stopped_callback) {
    orientation_stopped_callback_ = orientation_stopped_callback;
  }

  bool Start(ConsumerType consumer_type, void* buffer) override {
    EXPECT_TRUE(buffer);

    switch (consumer_type) {
      case CONSUMER_TYPE_MOTION: {
        DeviceMotionHardwareBuffer* motion_buffer =
            static_cast<DeviceMotionHardwareBuffer*>(buffer);
        if (sensor_data_available_)
          UpdateMotion(motion_buffer);
        SetMotionBufferReady(motion_buffer);
        BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                                motion_started_callback_);
      } break;
      case CONSUMER_TYPE_ORIENTATION: {
        DeviceOrientationHardwareBuffer* orientation_buffer =
            static_cast<DeviceOrientationHardwareBuffer*>(buffer);
        if (sensor_data_available_)
          UpdateOrientation(orientation_buffer);
        SetOrientationBufferReady(orientation_buffer);
        BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                                orientation_started_callback_);
      } break;
      case CONSUMER_TYPE_LIGHT: {
        DeviceLightHardwareBuffer* light_buffer =
            static_cast<DeviceLightHardwareBuffer*>(buffer);
        UpdateLight(light_buffer,
                    sensor_data_available_
                        ? 100
                        : std::numeric_limits<double>::infinity());
        BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                                light_started_callback_);
      } break;
      default:
        return false;
    }
    return true;
  }

  bool Stop(ConsumerType consumer_type) override {
    switch (consumer_type) {
      case CONSUMER_TYPE_MOTION:
        BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                                motion_stopped_callback_);
        break;
      case CONSUMER_TYPE_ORIENTATION:
        BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                                orientation_stopped_callback_);
        break;
      case CONSUMER_TYPE_LIGHT:
        BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                                light_stopped_callback_);
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

  // The below callbacks should be run on the UI thread.
  base::Closure motion_started_callback_;
  base::Closure orientation_started_callback_;
  base::Closure light_started_callback_;
  base::Closure motion_stopped_callback_;
  base::Closure orientation_stopped_callback_;
  base::Closure light_stopped_callback_;
  bool sensor_data_available_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeDataFetcher);
};

class DeviceSensorBrowserTest : public ContentBrowserTest {
 public:
  DeviceSensorBrowserTest()
      : fetcher_(nullptr),
        io_loop_finished_event_(
            base::WaitableEvent::ResetPolicy::AUTOMATIC,
            base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  void SetUpOnMainThread() override {
    // Initialize the RunLoops now that the main thread has been created.
    light_started_runloop_.reset(new base::RunLoop());
    light_stopped_runloop_.reset(new base::RunLoop());
    motion_started_runloop_.reset(new base::RunLoop());
    motion_stopped_runloop_.reset(new base::RunLoop());
    orientation_started_runloop_.reset(new base::RunLoop());
    orientation_stopped_runloop_.reset(new base::RunLoop());
#if defined(OS_ANDROID)
    // On Android, the DeviceSensorService lives on the UI thread.
    SetUpFetcher();
#else
    // On all other platforms, the DeviceSensorService lives on the IO thread.
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&DeviceSensorBrowserTest::SetUpOnIOThread,
                   base::Unretained(this)));
    io_loop_finished_event_.Wait();
#endif
  }

  void SetUpFetcher() {
    fetcher_ = new FakeDataFetcher();
    fetcher_->SetLightStartedCallback(light_started_runloop_->QuitClosure());
    fetcher_->SetLightStoppedCallback(light_stopped_runloop_->QuitClosure());
    fetcher_->SetMotionStartedCallback(motion_started_runloop_->QuitClosure());
    fetcher_->SetMotionStoppedCallback(motion_stopped_runloop_->QuitClosure());
    fetcher_->SetOrientationStartedCallback(
        orientation_started_runloop_->QuitClosure());
    fetcher_->SetOrientationStoppedCallback(
        orientation_stopped_runloop_->QuitClosure());
    DeviceSensorService::GetInstance()->SetDataFetcherForTesting(fetcher_);
  }

  void SetUpOnIOThread() {
    SetUpFetcher();
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
        base::Bind(&DeviceSensorBrowserTest::DelayAndQuit,
                   base::Unretained(this), delay));
    runner->Run();
  }

  void EnableExperimentalFeatures() {
    // TODO(riju): remove when the DeviceLight feature goes stable.
    base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
    if (!cmd_line->HasSwitch(switches::kEnableExperimentalWebPlatformFeatures))
      cmd_line->AppendSwitch(switches::kEnableExperimentalWebPlatformFeatures);
  }

  FakeDataFetcher* fetcher_;

  // NOTE: These can only be initialized once the main thread has been created
  // and so must be pointers instead of plain objects.
  std::unique_ptr<base::RunLoop> light_started_runloop_;
  std::unique_ptr<base::RunLoop> light_stopped_runloop_;
  std::unique_ptr<base::RunLoop> motion_started_runloop_;
  std::unique_ptr<base::RunLoop> motion_stopped_runloop_;
  std::unique_ptr<base::RunLoop> orientation_started_runloop_;
  std::unique_ptr<base::RunLoop> orientation_stopped_runloop_;

 private:
  base::WaitableEvent io_loop_finished_event_;
};

IN_PROC_BROWSER_TEST_F(DeviceSensorBrowserTest, OrientationTest) {
  // The test page will register an event handler for orientation events,
  // expects to get an event with fake values, then removes the event
  // handler and navigates to #pass.
  GURL test_url = GetTestUrl("device_sensors", "device_orientation_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);

  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
  orientation_started_runloop_->Run();
  orientation_stopped_runloop_->Run();
}

IN_PROC_BROWSER_TEST_F(DeviceSensorBrowserTest, LightTest) {
  // The test page will register an event handler for light events,
  // expects to get an event with fake values, then removes the event
  // handler and navigates to #pass.
  EnableExperimentalFeatures();
  GURL test_url = GetTestUrl("device_sensors", "device_light_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);

  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
  light_started_runloop_->Run();
  light_stopped_runloop_->Run();
}

IN_PROC_BROWSER_TEST_F(DeviceSensorBrowserTest, MotionTest) {
  // The test page will register an event handler for motion events,
  // expects to get an event with fake values, then removes the event
  // handler and navigates to #pass.
  GURL test_url = GetTestUrl("device_sensors", "device_motion_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);

  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
  motion_started_runloop_->Run();
  motion_stopped_runloop_->Run();
}

IN_PROC_BROWSER_TEST_F(DeviceSensorBrowserTest, LightOneOffInfintyTest) {
  // The test page registers an event handler for light events and expects
  // to get an event with value equal to infinity, because no sensor data can
  // be provided.
  EnableExperimentalFeatures();
  fetcher_->SetSensorDataAvailable(false);
  GURL test_url =
      GetTestUrl("device_sensors", "device_light_infinity_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);

  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
  light_started_runloop_->Run();
  light_stopped_runloop_->Run();
}

IN_PROC_BROWSER_TEST_F(DeviceSensorBrowserTest, OrientationNullTest) {
  // The test page registers an event handler for orientation events and
  // expects to get an event with null values, because no sensor data can be
  // provided.
  fetcher_->SetSensorDataAvailable(false);
  GURL test_url =
      GetTestUrl("device_sensors", "device_orientation_null_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);

  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
  orientation_started_runloop_->Run();
  orientation_stopped_runloop_->Run();
}

IN_PROC_BROWSER_TEST_F(DeviceSensorBrowserTest, MotionNullTest) {
  // The test page registers an event handler for motion events and
  // expects to get an event with null values, because no sensor data can be
  // provided.
  fetcher_->SetSensorDataAvailable(false);
  GURL test_url = GetTestUrl("device_sensors", "device_motion_null_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);

  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
  motion_started_runloop_->Run();
  motion_stopped_runloop_->Run();
}

IN_PROC_BROWSER_TEST_F(DeviceSensorBrowserTest, NullTestWithAlert) {
  // The test page registers an event handlers for motion/orientation events and
  // expects to get events with null values. The test raises a modal alert
  // dialog with a delay to test that the one-off null-events still propagate to
  // window after the alert is dismissed and the callbacks are invoked which
  // eventually navigate to #pass.
  fetcher_->SetSensorDataAvailable(false);
  TestNavigationObserver same_tab_observer(shell()->web_contents(), 2);

  GURL test_url =
      GetTestUrl("device_sensors", "device_sensors_null_test_with_alert.html");
  shell()->LoadURL(test_url);

  // TODO(timvolodine): investigate if it is possible to test this without
  // delay, crbug.com/360044.
  WaitForAlertDialogAndQuitAfterDelay(base::TimeDelta::FromMilliseconds(500));

  motion_started_runloop_->Run();
  motion_stopped_runloop_->Run();
  orientation_started_runloop_->Run();
  orientation_stopped_runloop_->Run();
  same_tab_observer.Wait();
  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
}

}  //  namespace

}  //  namespace content
