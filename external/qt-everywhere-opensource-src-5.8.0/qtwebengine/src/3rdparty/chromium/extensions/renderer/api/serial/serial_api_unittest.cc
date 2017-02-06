// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "device/serial/serial_device_enumerator.h"
#include "device/serial/serial_service_impl.h"
#include "device/serial/test_serial_io_handler.h"
#include "extensions/browser/mojo/stash_backend.h"
#include "extensions/common/mojo/keep_alive.mojom.h"
#include "extensions/renderer/api_test_base.h"
#include "grit/extensions_renderer_resources.h"

// A test launcher for tests for the serial API defined in
// extensions/test/data/serial_unittest.js. Each C++ test function sets up a
// fake DeviceEnumerator or SerialIoHandler expecting or returning particular
// values for that test.

namespace extensions {

namespace {

class FakeSerialDeviceEnumerator : public device::SerialDeviceEnumerator {
  mojo::Array<device::serial::DeviceInfoPtr> GetDevices() override {
    mojo::Array<device::serial::DeviceInfoPtr> result(3);
    result[0] = device::serial::DeviceInfo::New();
    result[0]->path = "device";
    result[0]->vendor_id = 1234;
    result[0]->has_vendor_id = true;
    result[0]->product_id = 5678;
    result[0]->has_product_id = true;
    result[0]->display_name = "foo";
    result[1] = device::serial::DeviceInfo::New();
    result[1]->path = "another_device";
    // These IDs should be ignored.
    result[1]->vendor_id = 1234;
    result[1]->product_id = 5678;
    result[2] = device::serial::DeviceInfo::New();
    result[2]->path = "";
    result[2]->display_name = "";
    return result;
  }
};

enum OptionalValue {
  OPTIONAL_VALUE_UNSET,
  OPTIONAL_VALUE_FALSE,
  OPTIONAL_VALUE_TRUE,
};

device::serial::HostControlSignals GenerateControlSignals(OptionalValue dtr,
                                                          OptionalValue rts) {
  device::serial::HostControlSignals result;
  switch (dtr) {
    case OPTIONAL_VALUE_UNSET:
      break;
    case OPTIONAL_VALUE_FALSE:
      result.dtr = false;
      result.has_dtr = true;
      break;
    case OPTIONAL_VALUE_TRUE:
      result.dtr = true;
      result.has_dtr = true;
      break;
  }
  switch (rts) {
    case OPTIONAL_VALUE_UNSET:
      break;
    case OPTIONAL_VALUE_FALSE:
      result.rts = false;
      result.has_rts = true;
      break;
    case OPTIONAL_VALUE_TRUE:
      result.rts = true;
      result.has_rts = true;
      break;
  }
  return result;
}

device::serial::ConnectionOptions GenerateConnectionOptions(
    int bitrate,
    device::serial::DataBits data_bits,
    device::serial::ParityBit parity_bit,
    device::serial::StopBits stop_bits,
    OptionalValue cts_flow_control) {
  device::serial::ConnectionOptions result;
  result.bitrate = bitrate;
  result.data_bits = data_bits;
  result.parity_bit = parity_bit;
  result.stop_bits = stop_bits;
  switch (cts_flow_control) {
    case OPTIONAL_VALUE_UNSET:
      break;
    case OPTIONAL_VALUE_FALSE:
      result.cts_flow_control = false;
      result.has_cts_flow_control = true;
      break;
    case OPTIONAL_VALUE_TRUE:
      result.cts_flow_control = true;
      result.has_cts_flow_control = true;
      break;
  }
  return result;
}

class TestIoHandlerBase : public device::TestSerialIoHandler {
 public:
  TestIoHandlerBase() : calls_(0) {}

  size_t num_calls() const { return calls_; }

 protected:
  ~TestIoHandlerBase() override {}
  void record_call() const { calls_++; }

 private:
  mutable size_t calls_;

  DISALLOW_COPY_AND_ASSIGN(TestIoHandlerBase);
};

class SetControlSignalsTestIoHandler : public TestIoHandlerBase {
 public:
  SetControlSignalsTestIoHandler() {}

  bool SetControlSignals(
      const device::serial::HostControlSignals& signals) override {
    static const device::serial::HostControlSignals expected_signals[] = {
        GenerateControlSignals(OPTIONAL_VALUE_UNSET, OPTIONAL_VALUE_UNSET),
        GenerateControlSignals(OPTIONAL_VALUE_FALSE, OPTIONAL_VALUE_UNSET),
        GenerateControlSignals(OPTIONAL_VALUE_TRUE, OPTIONAL_VALUE_UNSET),
        GenerateControlSignals(OPTIONAL_VALUE_UNSET, OPTIONAL_VALUE_FALSE),
        GenerateControlSignals(OPTIONAL_VALUE_FALSE, OPTIONAL_VALUE_FALSE),
        GenerateControlSignals(OPTIONAL_VALUE_TRUE, OPTIONAL_VALUE_FALSE),
        GenerateControlSignals(OPTIONAL_VALUE_UNSET, OPTIONAL_VALUE_TRUE),
        GenerateControlSignals(OPTIONAL_VALUE_FALSE, OPTIONAL_VALUE_TRUE),
        GenerateControlSignals(OPTIONAL_VALUE_TRUE, OPTIONAL_VALUE_TRUE),
    };
    if (num_calls() >= arraysize(expected_signals))
      return false;

    EXPECT_EQ(expected_signals[num_calls()].has_dtr, signals.has_dtr);
    EXPECT_EQ(expected_signals[num_calls()].dtr, signals.dtr);
    EXPECT_EQ(expected_signals[num_calls()].has_rts, signals.has_rts);
    EXPECT_EQ(expected_signals[num_calls()].rts, signals.rts);
    record_call();
    return true;
  }

 private:
  ~SetControlSignalsTestIoHandler() override {}

  DISALLOW_COPY_AND_ASSIGN(SetControlSignalsTestIoHandler);
};

class GetControlSignalsTestIoHandler : public TestIoHandlerBase {
 public:
  GetControlSignalsTestIoHandler() {}

  device::serial::DeviceControlSignalsPtr GetControlSignals() const override {
    device::serial::DeviceControlSignalsPtr signals(
        device::serial::DeviceControlSignals::New());
    signals->dcd = num_calls() & 1;
    signals->cts = num_calls() & 2;
    signals->ri = num_calls() & 4;
    signals->dsr = num_calls() & 8;
    record_call();
    return signals;
  }

 private:
  ~GetControlSignalsTestIoHandler() override {}

  DISALLOW_COPY_AND_ASSIGN(GetControlSignalsTestIoHandler);
};

class ConfigurePortTestIoHandler : public TestIoHandlerBase {
 public:
  ConfigurePortTestIoHandler() {}
  bool ConfigurePortImpl() override {
    static const device::serial::ConnectionOptions expected_options[] = {
        // Each JavaScript call to chrome.serial.update only modifies a single
        // property of the connection however this function can only check the
        // final value of all options. The modified option is marked with "set".
        GenerateConnectionOptions(9600, device::serial::DataBits::EIGHT,
                                  device::serial::ParityBit::NO,
                                  device::serial::StopBits::ONE,
                                  OPTIONAL_VALUE_FALSE),
        GenerateConnectionOptions(
            57600,  // set
            device::serial::DataBits::EIGHT, device::serial::ParityBit::NO,
            device::serial::StopBits::ONE, OPTIONAL_VALUE_FALSE),
        GenerateConnectionOptions(57600,
                                  device::serial::DataBits::SEVEN,  // set
                                  device::serial::ParityBit::NO,
                                  device::serial::StopBits::ONE,
                                  OPTIONAL_VALUE_FALSE),
        GenerateConnectionOptions(57600,
                                  device::serial::DataBits::EIGHT,  // set
                                  device::serial::ParityBit::NO,
                                  device::serial::StopBits::ONE,
                                  OPTIONAL_VALUE_FALSE),
        GenerateConnectionOptions(57600, device::serial::DataBits::EIGHT,
                                  device::serial::ParityBit::NO,  // set
                                  device::serial::StopBits::ONE,
                                  OPTIONAL_VALUE_FALSE),
        GenerateConnectionOptions(57600, device::serial::DataBits::EIGHT,
                                  device::serial::ParityBit::ODD,  // set
                                  device::serial::StopBits::ONE,
                                  OPTIONAL_VALUE_FALSE),
        GenerateConnectionOptions(57600, device::serial::DataBits::EIGHT,
                                  device::serial::ParityBit::EVEN,  // set
                                  device::serial::StopBits::ONE,
                                  OPTIONAL_VALUE_FALSE),
        GenerateConnectionOptions(57600, device::serial::DataBits::EIGHT,
                                  device::serial::ParityBit::EVEN,
                                  device::serial::StopBits::ONE,  // set
                                  OPTIONAL_VALUE_FALSE),
        GenerateConnectionOptions(57600, device::serial::DataBits::EIGHT,
                                  device::serial::ParityBit::EVEN,
                                  device::serial::StopBits::TWO,  // set
                                  OPTIONAL_VALUE_FALSE),
        GenerateConnectionOptions(57600, device::serial::DataBits::EIGHT,
                                  device::serial::ParityBit::EVEN,
                                  device::serial::StopBits::TWO,
                                  OPTIONAL_VALUE_FALSE),  // set
        GenerateConnectionOptions(57600, device::serial::DataBits::EIGHT,
                                  device::serial::ParityBit::EVEN,
                                  device::serial::StopBits::TWO,
                                  OPTIONAL_VALUE_TRUE),  // set
    };

    if (!TestIoHandlerBase::ConfigurePortImpl()) {
      return false;
    }

    if (num_calls() >= arraysize(expected_options)) {
      return false;
    }

    EXPECT_EQ(expected_options[num_calls()].bitrate, options().bitrate);
    EXPECT_EQ(expected_options[num_calls()].data_bits, options().data_bits);
    EXPECT_EQ(expected_options[num_calls()].parity_bit, options().parity_bit);
    EXPECT_EQ(expected_options[num_calls()].stop_bits, options().stop_bits);
    EXPECT_EQ(expected_options[num_calls()].has_cts_flow_control,
              options().has_cts_flow_control);
    EXPECT_EQ(expected_options[num_calls()].cts_flow_control,
              options().cts_flow_control);
    record_call();
    return true;
  }

 private:
  ~ConfigurePortTestIoHandler() override {}

  DISALLOW_COPY_AND_ASSIGN(ConfigurePortTestIoHandler);
};

class FlushTestIoHandler : public TestIoHandlerBase {
 public:
  FlushTestIoHandler() {}

  bool Flush() const override {
    record_call();
    return true;
  }

 private:
  ~FlushTestIoHandler() override {}

  DISALLOW_COPY_AND_ASSIGN(FlushTestIoHandler);
};

class FailToConnectTestIoHandler : public TestIoHandlerBase {
 public:
  FailToConnectTestIoHandler() {}
  void Open(const std::string& port,
            const device::serial::ConnectionOptions& options,
            const OpenCompleteCallback& callback) override {
    callback.Run(false);
    return;
  }

 private:
  ~FailToConnectTestIoHandler() override {}

  DISALLOW_COPY_AND_ASSIGN(FailToConnectTestIoHandler);
};

class FailToGetInfoTestIoHandler : public TestIoHandlerBase {
 public:
  explicit FailToGetInfoTestIoHandler(int times_to_succeed)
      : times_to_succeed_(times_to_succeed) {}
  device::serial::ConnectionInfoPtr GetPortInfo() const override {
    if (times_to_succeed_-- > 0)
      return device::TestSerialIoHandler::GetPortInfo();
    return device::serial::ConnectionInfoPtr();
  }

 private:
  ~FailToGetInfoTestIoHandler() override {}

  mutable int times_to_succeed_;

  DISALLOW_COPY_AND_ASSIGN(FailToGetInfoTestIoHandler);
};

class SendErrorTestIoHandler : public TestIoHandlerBase {
 public:
  explicit SendErrorTestIoHandler(device::serial::SendError error)
      : error_(error) {}

  void WriteImpl() override { QueueWriteCompleted(0, error_); }

 private:
  ~SendErrorTestIoHandler() override {}

  device::serial::SendError error_;

  DISALLOW_COPY_AND_ASSIGN(SendErrorTestIoHandler);
};

class FixedDataReceiveTestIoHandler : public TestIoHandlerBase {
 public:
  explicit FixedDataReceiveTestIoHandler(const std::string& data)
      : data_(data) {}

  void ReadImpl() override {
    if (pending_read_buffer_len() < data_.size())
      return;
    memcpy(pending_read_buffer(), data_.c_str(), data_.size());
    QueueReadCompleted(static_cast<uint32_t>(data_.size()),
                       device::serial::ReceiveError::NONE);
  }

 private:
  ~FixedDataReceiveTestIoHandler() override {}

  const std::string data_;

  DISALLOW_COPY_AND_ASSIGN(FixedDataReceiveTestIoHandler);
};

class ReceiveErrorTestIoHandler : public TestIoHandlerBase {
 public:
  explicit ReceiveErrorTestIoHandler(device::serial::ReceiveError error)
      : error_(error) {}

  void ReadImpl() override { QueueReadCompleted(0, error_); }

 private:
  ~ReceiveErrorTestIoHandler() override {}

  device::serial::ReceiveError error_;

  DISALLOW_COPY_AND_ASSIGN(ReceiveErrorTestIoHandler);
};

class SendDataWithErrorIoHandler : public TestIoHandlerBase {
 public:
  SendDataWithErrorIoHandler() : sent_error_(false) {}
  void WriteImpl() override {
    if (sent_error_) {
      WriteCompleted(pending_write_buffer_len(),
                     device::serial::SendError::NONE);
      return;
    }
    sent_error_ = true;
    // We expect the JS test code to send a 4 byte buffer.
    ASSERT_LT(2u, pending_write_buffer_len());
    WriteCompleted(2, device::serial::SendError::SYSTEM_ERROR);
  }

 private:
  ~SendDataWithErrorIoHandler() override {}

  bool sent_error_;

  DISALLOW_COPY_AND_ASSIGN(SendDataWithErrorIoHandler);
};

class BlockSendsForeverSendIoHandler : public TestIoHandlerBase {
 public:
  BlockSendsForeverSendIoHandler() {}
  void WriteImpl() override {}

 private:
  ~BlockSendsForeverSendIoHandler() override {}

  DISALLOW_COPY_AND_ASSIGN(BlockSendsForeverSendIoHandler);
};

}  // namespace

class SerialApiTest : public ApiTestBase {
 public:
  SerialApiTest() {}

  void SetUp() override {
    ApiTestBase::SetUp();
    stash_backend_.reset(new StashBackend(base::Closure()));
    PrepareEnvironment(api_test_env(), stash_backend_.get());
  }

  void PrepareEnvironment(ApiTestEnvironment* environment,
                          StashBackend* stash_backend) {
    environment->env()->RegisterModule("serial", IDR_SERIAL_CUSTOM_BINDINGS_JS);
    environment->service_provider()->AddService<device::serial::SerialService>(
        base::Bind(&SerialApiTest::CreateSerialService,
                   base::Unretained(this)));
    environment->service_provider()->AddService(base::Bind(
        &StashBackend::BindToRequest, base::Unretained(stash_backend)));
    environment->service_provider()->IgnoreServiceRequests<KeepAlive>();
  }

  scoped_refptr<TestIoHandlerBase> io_handler_;

  std::unique_ptr<StashBackend> stash_backend_;

 private:
  scoped_refptr<device::SerialIoHandler> GetIoHandler() {
    if (!io_handler_.get())
      io_handler_ = new TestIoHandlerBase;
    return io_handler_;
  }

  void CreateSerialService(
      mojo::InterfaceRequest<device::serial::SerialService> request) {
    new device::SerialServiceImpl(
        new device::SerialConnectionFactory(
            base::Bind(&SerialApiTest::GetIoHandler, base::Unretained(this)),
            base::ThreadTaskRunnerHandle::Get()),
        std::unique_ptr<device::SerialDeviceEnumerator>(
            new FakeSerialDeviceEnumerator),
        std::move(request));
  }

  DISALLOW_COPY_AND_ASSIGN(SerialApiTest);
};

TEST_F(SerialApiTest, GetDevices) {
  RunTest("serial_unittest.js", "testGetDevices");
}

TEST_F(SerialApiTest, ConnectFail) {
  io_handler_ = new FailToConnectTestIoHandler;
  RunTest("serial_unittest.js", "testConnectFail");
}

TEST_F(SerialApiTest, GetInfoFailOnConnect) {
  io_handler_ = new FailToGetInfoTestIoHandler(0);
  RunTest("serial_unittest.js", "testGetInfoFailOnConnect");
}

TEST_F(SerialApiTest, Connect) {
  RunTest("serial_unittest.js", "testConnect");
}

TEST_F(SerialApiTest, ConnectDefaultOptions) {
  RunTest("serial_unittest.js", "testConnectDefaultOptions");
}

TEST_F(SerialApiTest, ConnectInvalidBitrate) {
  RunTest("serial_unittest.js", "testConnectInvalidBitrate");
}

TEST_F(SerialApiTest, GetInfo) {
  RunTest("serial_unittest.js", "testGetInfo");
}

TEST_F(SerialApiTest, GetInfoAfterSerialization) {
  RunTest("serial_unittest.js", "testGetInfoAfterSerialization");
}

TEST_F(SerialApiTest, GetInfoFailToGetPortInfo) {
  io_handler_ = new FailToGetInfoTestIoHandler(1);
  RunTest("serial_unittest.js", "testGetInfoFailToGetPortInfo");
}

TEST_F(SerialApiTest, GetConnections) {
  RunTest("serial_unittest.js", "testGetConnections");
}

// https://crbug.com/599898
#if defined(LEAK_SANITIZER)
#define MAYBE_GetControlSignals DISABLED_GetControlSignals
#else
#define MAYBE_GetControlSignals GetControlSignals
#endif
TEST_F(SerialApiTest, MAYBE_GetControlSignals) {
  io_handler_ = new GetControlSignalsTestIoHandler;
  RunTest("serial_unittest.js", "testGetControlSignals");
  EXPECT_EQ(16u, io_handler_->num_calls());
}

TEST_F(SerialApiTest, SetControlSignals) {
  io_handler_ = new SetControlSignalsTestIoHandler;
  RunTest("serial_unittest.js", "testSetControlSignals");
  EXPECT_EQ(9u, io_handler_->num_calls());
}

// https://crbug.com/599898
#if defined(LEAK_SANITIZER)
#define MAYBE_Update DISABLED_Update
#else
#define MAYBE_Update Update
#endif
TEST_F(SerialApiTest, MAYBE_Update) {
  io_handler_ = new ConfigurePortTestIoHandler;
  RunTest("serial_unittest.js", "testUpdate");
  EXPECT_EQ(11u, io_handler_->num_calls());
}

// https://crbug.com/599898
#if defined(LEAK_SANITIZER)
#define MAYBE_UpdateAcrossSerialization DISABLED_UpdateAcrossSerialization
#else
#define MAYBE_UpdateAcrossSerialization UpdateAcrossSerialization
#endif
TEST_F(SerialApiTest, MAYBE_UpdateAcrossSerialization) {
  io_handler_ = new ConfigurePortTestIoHandler;
  RunTest("serial_unittest.js", "testUpdateAcrossSerialization");
  EXPECT_EQ(11u, io_handler_->num_calls());
}

TEST_F(SerialApiTest, UpdateInvalidBitrate) {
  io_handler_ = new ConfigurePortTestIoHandler;
  RunTest("serial_unittest.js", "testUpdateInvalidBitrate");
  EXPECT_EQ(1u, io_handler_->num_calls());
}

TEST_F(SerialApiTest, Flush) {
  io_handler_ = new FlushTestIoHandler;
  RunTest("serial_unittest.js", "testFlush");
  EXPECT_EQ(1u, io_handler_->num_calls());
}

TEST_F(SerialApiTest, SetPaused) {
  RunTest("serial_unittest.js", "testSetPaused");
}

TEST_F(SerialApiTest, Echo) {
  RunTest("serial_unittest.js", "testEcho");
}

TEST_F(SerialApiTest, EchoAfterSerialization) {
  RunTest("serial_unittest.js", "testEchoAfterSerialization");
}

TEST_F(SerialApiTest, SendDuringExistingSend) {
  RunTest("serial_unittest.js", "testSendDuringExistingSend");
}

TEST_F(SerialApiTest, SendAfterSuccessfulSend) {
  RunTest("serial_unittest.js", "testSendAfterSuccessfulSend");
}

TEST_F(SerialApiTest, SendPartialSuccessWithError) {
  io_handler_ = new SendDataWithErrorIoHandler();
  RunTest("serial_unittest.js", "testSendPartialSuccessWithError");
}

TEST_F(SerialApiTest, SendTimeout) {
  io_handler_ = new BlockSendsForeverSendIoHandler();
  RunTest("serial_unittest.js", "testSendTimeout");
}

TEST_F(SerialApiTest, SendTimeoutAfterSerialization) {
  io_handler_ = new BlockSendsForeverSendIoHandler();
  RunTest("serial_unittest.js", "testSendTimeoutAfterSerialization");
}

TEST_F(SerialApiTest, DisableSendTimeout) {
  io_handler_ = new BlockSendsForeverSendIoHandler();
  RunTest("serial_unittest.js", "testDisableSendTimeout");
}

TEST_F(SerialApiTest, PausedReceive) {
  io_handler_ = new FixedDataReceiveTestIoHandler("data");
  RunTest("serial_unittest.js", "testPausedReceive");
}

TEST_F(SerialApiTest, PausedReceiveError) {
  io_handler_ =
      new ReceiveErrorTestIoHandler(device::serial::ReceiveError::DEVICE_LOST);
  RunTest("serial_unittest.js", "testPausedReceiveError");
}

TEST_F(SerialApiTest, ReceiveTimeout) {
  RunTest("serial_unittest.js", "testReceiveTimeout");
}

TEST_F(SerialApiTest, ReceiveTimeoutAfterSerialization) {
  RunTest("serial_unittest.js", "testReceiveTimeoutAfterSerialization");
}

TEST_F(SerialApiTest, DisableReceiveTimeout) {
  RunTest("serial_unittest.js", "testDisableReceiveTimeout");
}

TEST_F(SerialApiTest, ReceiveErrorDisconnected) {
  io_handler_ =
      new ReceiveErrorTestIoHandler(device::serial::ReceiveError::DISCONNECTED);
  RunTest("serial_unittest.js", "testReceiveErrorDisconnected");
}

TEST_F(SerialApiTest, ReceiveErrorDeviceLost) {
  io_handler_ =
      new ReceiveErrorTestIoHandler(device::serial::ReceiveError::DEVICE_LOST);
  RunTest("serial_unittest.js", "testReceiveErrorDeviceLost");
}

TEST_F(SerialApiTest, ReceiveErrorBreak) {
  io_handler_ =
      new ReceiveErrorTestIoHandler(device::serial::ReceiveError::BREAK);
  RunTest("serial_unittest.js", "testReceiveErrorBreak");
}

TEST_F(SerialApiTest, ReceiveErrorFrameError) {
  io_handler_ =
      new ReceiveErrorTestIoHandler(device::serial::ReceiveError::FRAME_ERROR);
  RunTest("serial_unittest.js", "testReceiveErrorFrameError");
}

TEST_F(SerialApiTest, ReceiveErrorOverrun) {
  io_handler_ =
      new ReceiveErrorTestIoHandler(device::serial::ReceiveError::OVERRUN);
  RunTest("serial_unittest.js", "testReceiveErrorOverrun");
}

TEST_F(SerialApiTest, ReceiveErrorBufferOverflow) {
  io_handler_ = new ReceiveErrorTestIoHandler(
      device::serial::ReceiveError::BUFFER_OVERFLOW);
  RunTest("serial_unittest.js", "testReceiveErrorBufferOverflow");
}

TEST_F(SerialApiTest, ReceiveErrorParityError) {
  io_handler_ =
      new ReceiveErrorTestIoHandler(device::serial::ReceiveError::PARITY_ERROR);
  RunTest("serial_unittest.js", "testReceiveErrorParityError");
}

TEST_F(SerialApiTest, ReceiveErrorSystemError) {
  io_handler_ =
      new ReceiveErrorTestIoHandler(device::serial::ReceiveError::SYSTEM_ERROR);
  RunTest("serial_unittest.js", "testReceiveErrorSystemError");
}

TEST_F(SerialApiTest, SendErrorDisconnected) {
  io_handler_ =
      new SendErrorTestIoHandler(device::serial::SendError::DISCONNECTED);
  RunTest("serial_unittest.js", "testSendErrorDisconnected");
}

TEST_F(SerialApiTest, SendErrorSystemError) {
  io_handler_ =
      new SendErrorTestIoHandler(device::serial::SendError::SYSTEM_ERROR);
  RunTest("serial_unittest.js", "testSendErrorSystemError");
}

TEST_F(SerialApiTest, DisconnectUnknownConnectionId) {
  RunTest("serial_unittest.js", "testDisconnectUnknownConnectionId");
}

TEST_F(SerialApiTest, GetInfoUnknownConnectionId) {
  RunTest("serial_unittest.js", "testGetInfoUnknownConnectionId");
}

TEST_F(SerialApiTest, UpdateUnknownConnectionId) {
  RunTest("serial_unittest.js", "testUpdateUnknownConnectionId");
}

TEST_F(SerialApiTest, SetControlSignalsUnknownConnectionId) {
  RunTest("serial_unittest.js", "testSetControlSignalsUnknownConnectionId");
}

TEST_F(SerialApiTest, GetControlSignalsUnknownConnectionId) {
  RunTest("serial_unittest.js", "testGetControlSignalsUnknownConnectionId");
}

TEST_F(SerialApiTest, FlushUnknownConnectionId) {
  RunTest("serial_unittest.js", "testFlushUnknownConnectionId");
}

TEST_F(SerialApiTest, SetPausedUnknownConnectionId) {
  RunTest("serial_unittest.js", "testSetPausedUnknownConnectionId");
}

TEST_F(SerialApiTest, SendUnknownConnectionId) {
  RunTest("serial_unittest.js", "testSendUnknownConnectionId");
}

// Note: these tests are disabled, since there is no good story for persisting
// the stashed handles when an extension process is shut down. See
// https://crbug.com/538774
TEST_F(SerialApiTest, DISABLED_StashAndRestoreDuringEcho) {
  ASSERT_NO_FATAL_FAILURE(RunTest("serial_unittest.js", "testSendAndStash"));
  std::unique_ptr<ModuleSystemTestEnvironment> new_env(CreateEnvironment());
  ApiTestEnvironment new_api_test_env(new_env.get());
  PrepareEnvironment(&new_api_test_env, stash_backend_.get());
  new_api_test_env.RunTest("serial_unittest.js", "testRestoreAndReceive");
}

TEST_F(SerialApiTest, DISABLED_StashAndRestoreDuringEchoError) {
  io_handler_ =
      new ReceiveErrorTestIoHandler(device::serial::ReceiveError::DEVICE_LOST);
  ASSERT_NO_FATAL_FAILURE(
      RunTest("serial_unittest.js", "testRestoreAndReceiveErrorSetUp"));
  std::unique_ptr<ModuleSystemTestEnvironment> new_env(CreateEnvironment());
  ApiTestEnvironment new_api_test_env(new_env.get());
  PrepareEnvironment(&new_api_test_env, stash_backend_.get());
  new_api_test_env.RunTest("serial_unittest.js", "testRestoreAndReceiveError");
}

// https://crbug.com/599898
#if defined(LEAK_SANITIZER)
#define MAYBE_StashAndRestoreNoConnections DISABLED_StashAndRestoreNoConnections
#else
#define MAYBE_StashAndRestoreNoConnections StashAndRestoreNoConnections
#endif
TEST_F(SerialApiTest, MAYBE_StashAndRestoreNoConnections) {
  ASSERT_NO_FATAL_FAILURE(
      RunTest("serial_unittest.js", "testStashNoConnections"));
  io_handler_ = nullptr;
  std::unique_ptr<ModuleSystemTestEnvironment> new_env(CreateEnvironment());
  ApiTestEnvironment new_api_test_env(new_env.get());
  PrepareEnvironment(&new_api_test_env, stash_backend_.get());
  new_api_test_env.RunTest("serial_unittest.js", "testRestoreNoConnections");
}

}  // namespace extensions
