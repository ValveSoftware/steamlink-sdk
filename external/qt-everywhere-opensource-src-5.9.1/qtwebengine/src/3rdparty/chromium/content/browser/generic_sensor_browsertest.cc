// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_javascript_dialog_manager.h"
#include "device/generic_sensor/platform_sensor.h"
#include "device/generic_sensor/platform_sensor_provider.h"
#include "device/generic_sensor/sensor_provider_impl.h"

namespace content {

namespace {

class FakeAmbientLightSensor : public device::PlatformSensor {
 public:
  FakeAmbientLightSensor(device::mojom::SensorType type,
                         mojo::ScopedSharedBufferMapping mapping,
                         device::PlatformSensorProvider* provider)
      : PlatformSensor(type, std::move(mapping), provider) {}

  device::mojom::ReportingMode GetReportingMode() override {
    return device::mojom::ReportingMode::ON_CHANGE;
  }

  bool StartSensor(
      const device::PlatformSensorConfiguration& configuration) override {
    device::SensorReading reading;
    reading.timestamp =
        (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();
    reading.values[0] = 50;
    UpdateSensorReading(reading, true);
    return true;
  }

  void StopSensor() override{};

 protected:
  ~FakeAmbientLightSensor() override = default;
  bool CheckSensorConfiguration(
      const device::PlatformSensorConfiguration& configuration) override {
    return true;
  }
  device::PlatformSensorConfiguration GetDefaultConfiguration() override {
    device::PlatformSensorConfiguration default_configuration;
    default_configuration.set_frequency(60);
    return default_configuration;
  }
};

class FakeSensorProvider : public device::PlatformSensorProvider {
 public:
  static FakeSensorProvider* GetInstance() {
    return base::Singleton<FakeSensorProvider, base::LeakySingletonTraits<
                                                   FakeSensorProvider>>::get();
  }
  FakeSensorProvider() = default;
  ~FakeSensorProvider() override = default;

 protected:
  void CreateSensorInternal(device::mojom::SensorType type,
                            mojo::ScopedSharedBufferMapping mapping,
                            const CreateSensorCallback& callback) override {
    // Create Sensors here.
    switch (type) {
      case device::mojom::SensorType::AMBIENT_LIGHT: {
        scoped_refptr<device::PlatformSensor> sensor =
            new FakeAmbientLightSensor(type, std::move(mapping), this);
        callback.Run(std::move(sensor));
        break;
      }
      default:
        NOTIMPLEMENTED();
        callback.Run(nullptr);
    }
  }
};

class GenericSensorBrowserTest : public ContentBrowserTest {
 public:
  GenericSensorBrowserTest()
      : io_loop_finished_event_(
            base::WaitableEvent::ResetPolicy::AUTOMATIC,
            base::WaitableEvent::InitialState::NOT_SIGNALED) {
    // TODO(darktears): remove when the GenericSensor feature goes stable.
    base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
    cmd_line->AppendSwitchASCII(switches::kEnableFeatures, "GenericSensor");
  }

  void SetUpOnMainThread() override {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&GenericSensorBrowserTest::SetUpOnIOThread,
                   base::Unretained(this)));
    io_loop_finished_event_.Wait();
  }

  void SetUpOnIOThread() {
    device::PlatformSensorProvider::SetProviderForTesting(
        FakeSensorProvider::GetInstance());
    io_loop_finished_event_.Signal();
  }

  void TearDown() override {
    device::PlatformSensorProvider::SetProviderForTesting(nullptr);
  }

 public:
  base::WaitableEvent io_loop_finished_event_;
};

IN_PROC_BROWSER_TEST_F(GenericSensorBrowserTest, AmbientLightSensorTest) {
  // The test page will create an AmbientLightSensor object in Javascript,
  // expects to get events with fake values then navigates to #pass.
  GURL test_url =
      GetTestUrl("generic_sensor", "ambient_light_sensor_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);
  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
}

}  //  namespace

}  //  namespace content
