// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/browser/browser_thread.h"
#include "device/serial/serial_device_enumerator.h"
#include "device/serial/serial_service_impl.h"
#include "device/serial/test_serial_io_handler.h"
#include "extensions/browser/api/serial/serial_api.h"
#include "extensions/browser/api/serial/serial_connection.h"
#include "extensions/browser/api/serial/serial_service_factory.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/api/serial.h"
#include "extensions/common/switches.h"
#include "extensions/test/result_catcher.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::Return;

namespace extensions {
namespace {

class FakeSerialGetDevicesFunction : public AsyncExtensionFunction {
 public:
  bool RunAsync() override {
    std::unique_ptr<base::ListValue> devices(new base::ListValue());
    std::unique_ptr<base::DictionaryValue> device0(new base::DictionaryValue());
    device0->SetString("path", "/dev/fakeserial");
    std::unique_ptr<base::DictionaryValue> device1(new base::DictionaryValue());
    device1->SetString("path", "\\\\COM800\\");
    devices->Append(std::move(device0));
    devices->Append(std::move(device1));
    SetResult(std::move(devices));
    SendResponse(true);
    return true;
  }

 protected:
  ~FakeSerialGetDevicesFunction() override {}
};

class FakeSerialDeviceEnumerator : public device::SerialDeviceEnumerator {
 public:
  ~FakeSerialDeviceEnumerator() override {}

  mojo::Array<device::serial::DeviceInfoPtr> GetDevices() override {
    mojo::Array<device::serial::DeviceInfoPtr> devices;
    device::serial::DeviceInfoPtr device0(device::serial::DeviceInfo::New());
    device0->path = "/dev/fakeserialmojo";
    device::serial::DeviceInfoPtr device1(device::serial::DeviceInfo::New());
    device1->path = "\\\\COM800\\";
    devices.push_back(std::move(device0));
    devices.push_back(std::move(device1));
    return devices;
  }
};

class FakeEchoSerialIoHandler : public device::TestSerialIoHandler {
 public:
  FakeEchoSerialIoHandler() {
    device_control_signals()->dcd = true;
    device_control_signals()->cts = true;
    device_control_signals()->ri = true;
    device_control_signals()->dsr = true;
    EXPECT_CALL(*this, SetControlSignals(_)).Times(1).WillOnce(Return(true));
  }

  static scoped_refptr<device::SerialIoHandler> Create() {
    return new FakeEchoSerialIoHandler();
  }

  MOCK_METHOD1(SetControlSignals,
               bool(const device::serial::HostControlSignals&));

 protected:
  ~FakeEchoSerialIoHandler() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeEchoSerialIoHandler);
};

class FakeSerialConnectFunction : public api::SerialConnectFunction {
 protected:
  SerialConnection* CreateSerialConnection(
      const std::string& port,
      const std::string& owner_extension_id) const override {
    scoped_refptr<FakeEchoSerialIoHandler> io_handler =
        new FakeEchoSerialIoHandler;
    SerialConnection* serial_connection =
        new SerialConnection(port, owner_extension_id);
    serial_connection->SetIoHandlerForTest(io_handler);
    return serial_connection;
  }

 protected:
  ~FakeSerialConnectFunction() override {}
};

class SerialApiTest : public ExtensionApiTest,
                      public testing::WithParamInterface<bool> {
 public:
  SerialApiTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    if (GetParam())
      command_line->AppendSwitch(switches::kEnableMojoSerialService);
  }

  void TearDownOnMainThread() override {
    SetSerialServiceFactoryForTest(nullptr);
    ExtensionApiTest::TearDownOnMainThread();
  }

 protected:
  base::Callback<void(mojo::InterfaceRequest<device::serial::SerialService>)>
      serial_service_factory_;
};

ExtensionFunction* FakeSerialGetDevicesFunctionFactory() {
  return new FakeSerialGetDevicesFunction();
}

ExtensionFunction* FakeSerialConnectFunctionFactory() {
  return new FakeSerialConnectFunction();
}

void CreateTestSerialServiceOnFileThread(
    mojo::InterfaceRequest<device::serial::SerialService> request) {
  auto io_handler_factory = base::Bind(&FakeEchoSerialIoHandler::Create);
  auto* connection_factory = new device::SerialConnectionFactory(
      io_handler_factory, content::BrowserThread::GetMessageLoopProxyForThread(
                              content::BrowserThread::IO));
  std::unique_ptr<device::SerialDeviceEnumerator> device_enumerator(
      new FakeSerialDeviceEnumerator);
  new device::SerialServiceImpl(
      connection_factory, std::move(device_enumerator), std::move(request));
}

void CreateTestSerialService(
    mojo::InterfaceRequest<device::serial::SerialService> request) {
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&CreateTestSerialServiceOnFileThread, base::Passed(&request)));
}

}  // namespace

// Disable SIMULATE_SERIAL_PORTS only if all the following are true:
//
// 1. You have an Arduino or compatible board attached to your machine and
// properly appearing as the first virtual serial port ("first" is very loosely
// defined as whichever port shows up in serial.getPorts). We've tested only
// the Atmega32u4 Breakout Board and Arduino Leonardo; note that both these
// boards are based on the Atmel ATmega32u4, rather than the more common
// Arduino '328p with either FTDI or '8/16u2 USB interfaces. TODO: test more
// widely.
//
// 2. Your user has permission to read/write the port. For example, this might
// mean that your user is in the "tty" or "uucp" group on Ubuntu flavors of
// Linux, or else that the port's path (e.g., /dev/ttyACM0) has global
// read/write permissions.
//
// 3. You have uploaded a program to the board that does a byte-for-byte echo
// on the virtual serial port at 57600 bps. An example is at
// chrome/test/data/extensions/api_test/serial/api/serial_arduino_test.ino.
//
#define SIMULATE_SERIAL_PORTS (1)
IN_PROC_BROWSER_TEST_P(SerialApiTest, SerialFakeHardware) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

#if SIMULATE_SERIAL_PORTS
  if (GetParam()) {
    serial_service_factory_ = base::Bind(&CreateTestSerialService);
    SetSerialServiceFactoryForTest(&serial_service_factory_);
  } else {
    ASSERT_TRUE(ExtensionFunctionDispatcher::OverrideFunction(
        "serial.getDevices", FakeSerialGetDevicesFunctionFactory));
    ASSERT_TRUE(ExtensionFunctionDispatcher::OverrideFunction(
        "serial.connect", FakeSerialConnectFunctionFactory));
  }
#endif

  ASSERT_TRUE(RunExtensionTest("serial/api")) << message_;
}

IN_PROC_BROWSER_TEST_P(SerialApiTest, SerialRealHardware) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  ASSERT_TRUE(RunExtensionTest("serial/real_hardware")) << message_;
}

INSTANTIATE_TEST_CASE_P(SerialApiTest, SerialApiTest, testing::Bool());

}  // namespace extensions
