// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "device/serial/serial.mojom.h"
#include "device/serial/serial_service_impl.h"
#include "device/serial/test_serial_io_handler.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {
namespace {

class FakeSerialDeviceEnumerator : public SerialDeviceEnumerator {
  mojo::Array<serial::DeviceInfoPtr> GetDevices() override {
    mojo::Array<serial::DeviceInfoPtr> devices(1);
    devices[0] = serial::DeviceInfo::New();
    devices[0]->path = "device";
    return devices;
  }
};

class FailToOpenIoHandler : public TestSerialIoHandler {
 public:
  void Open(const std::string& port,
            const serial::ConnectionOptions& options,
            const OpenCompleteCallback& callback) override {
    callback.Run(false);
  }

 protected:
  ~FailToOpenIoHandler() override {}
};

}  // namespace

class SerialServiceTest : public testing::Test {
 public:
  SerialServiceTest() : connected_(false), expecting_error_(false) {}

  void StoreDevices(mojo::Array<serial::DeviceInfoPtr> devices) {
    devices_ = std::move(devices);
    StopMessageLoop();
  }

  void OnConnectionError() {
    StopMessageLoop();
    EXPECT_TRUE(expecting_error_);
  }

  void RunMessageLoop() {
    run_loop_.reset(new base::RunLoop);
    run_loop_->Run();
  }

  void StopMessageLoop() {
    ASSERT_TRUE(run_loop_);
    message_loop_.task_runner()->PostTask(FROM_HERE, run_loop_->QuitClosure());
  }

  void OnGotInfo(serial::ConnectionInfoPtr options) {
    connected_ = true;
    StopMessageLoop();
  }

  scoped_refptr<SerialIoHandler> ReturnIoHandler() { return io_handler_; }

  void RunConnectTest(const std::string& path, bool expecting_success) {
    if (!io_handler_.get())
      io_handler_ = new TestSerialIoHandler;
    mojo::InterfacePtr<serial::SerialService> service;
    new SerialServiceImpl(
        new SerialConnectionFactory(
            base::Bind(&SerialServiceTest::ReturnIoHandler,
                       base::Unretained(this)),
            base::ThreadTaskRunnerHandle::Get()),
        std::unique_ptr<SerialDeviceEnumerator>(new FakeSerialDeviceEnumerator),
        mojo::GetProxy(&service));
    mojo::InterfacePtr<serial::Connection> connection;
    mojo::InterfacePtr<serial::DataSink> sink;
    mojo::InterfacePtr<serial::DataSource> source;
    mojo::InterfacePtr<serial::DataSourceClient> source_client;
    mojo::GetProxy(&source_client);
    service->Connect(path, serial::ConnectionOptions::New(),
                     mojo::GetProxy(&connection), mojo::GetProxy(&sink),
                     mojo::GetProxy(&source), std::move(source_client));
    connection.set_connection_error_handler(base::Bind(
        &SerialServiceTest::OnConnectionError, base::Unretained(this)));
    expecting_error_ = !expecting_success;
    connection->GetInfo(
        base::Bind(&SerialServiceTest::OnGotInfo, base::Unretained(this)));
    RunMessageLoop();
    EXPECT_EQ(!expecting_success, connection.encountered_error());
    EXPECT_EQ(expecting_success, connected_);
    connection.reset();
  }

  base::MessageLoop message_loop_;
  std::unique_ptr<base::RunLoop> run_loop_;
  mojo::Array<serial::DeviceInfoPtr> devices_;
  scoped_refptr<TestSerialIoHandler> io_handler_;
  bool connected_;
  bool expecting_error_;
  serial::ConnectionInfoPtr info_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SerialServiceTest);
};

TEST_F(SerialServiceTest, GetDevices) {
  mojo::InterfacePtr<serial::SerialService> service;
  SerialServiceImpl::Create(NULL, NULL, mojo::GetProxy(&service));
  service.set_connection_error_handler(base::Bind(
      &SerialServiceTest::OnConnectionError, base::Unretained(this)));
  mojo::Array<serial::DeviceInfoPtr> result;
  service->GetDevices(
      base::Bind(&SerialServiceTest::StoreDevices, base::Unretained(this)));
  RunMessageLoop();

  // Because we're running on unknown hardware, only check that we received a
  // non-null result.
  EXPECT_TRUE(devices_);
}

TEST_F(SerialServiceTest, Connect) {
  RunConnectTest("device", true);
}

TEST_F(SerialServiceTest, ConnectInvalidPath) {
  RunConnectTest("invalid_path", false);
}

TEST_F(SerialServiceTest, ConnectOpenFailed) {
  io_handler_ = new FailToOpenIoHandler;
  RunConnectTest("device", false);
}

}  // namespace device
