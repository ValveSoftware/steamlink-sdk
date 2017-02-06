// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/serial/serial_connection.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "device/serial/data_receiver.h"
#include "device/serial/data_sender.h"
#include "device/serial/data_stream.mojom.h"
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

}  // namespace

class SerialConnectionTest : public testing::Test {
 public:
  enum Event {
    EVENT_NONE,
    EVENT_GOT_INFO,
    EVENT_SET_OPTIONS,
    EVENT_GOT_CONTROL_SIGNALS,
    EVENT_SET_CONTROL_SIGNALS,
    EVENT_FLUSHED,
    EVENT_DATA_AT_IO_HANDLER,
    EVENT_DATA_SENT,
    EVENT_SEND_ERROR,
    EVENT_DATA_RECEIVED,
    EVENT_RECEIVE_ERROR,
    EVENT_CANCEL_COMPLETE,
    EVENT_ERROR,
  };

  static const uint32_t kBufferSize;

  SerialConnectionTest()
      : connected_(false),
        success_(false),
        bytes_sent_(0),
        send_error_(serial::SendError::NONE),
        receive_error_(serial::ReceiveError::NONE),
        expected_event_(EVENT_NONE) {}

  void SetUp() override {
    message_loop_.reset(new base::MessageLoop);
    mojo::InterfacePtr<serial::SerialService> service;
    new SerialServiceImpl(
        new SerialConnectionFactory(
            base::Bind(&SerialConnectionTest::CreateIoHandler,
                       base::Unretained(this)),
            base::ThreadTaskRunnerHandle::Get()),
        std::unique_ptr<SerialDeviceEnumerator>(new FakeSerialDeviceEnumerator),
        mojo::GetProxy(&service));
    service.set_connection_error_handler(base::Bind(
        &SerialConnectionTest::OnConnectionError, base::Unretained(this)));
    mojo::InterfacePtr<serial::DataSink> sink;
    mojo::InterfacePtr<serial::DataSource> source;
    mojo::InterfacePtr<serial::DataSourceClient> source_client;
    mojo::InterfaceRequest<serial::DataSourceClient> source_client_request =
        mojo::GetProxy(&source_client);
    service->Connect("device", serial::ConnectionOptions::New(),
                     mojo::GetProxy(&connection_), mojo::GetProxy(&sink),
                     mojo::GetProxy(&source), std::move(source_client));
    sender_.reset(
        new DataSender(std::move(sink), kBufferSize,
                       static_cast<int32_t>(serial::SendError::DISCONNECTED)));
    receiver_ = new DataReceiver(
        std::move(source), std::move(source_client_request), kBufferSize,
        static_cast<int32_t>(serial::ReceiveError::DISCONNECTED));
    connection_.set_connection_error_handler(base::Bind(
        &SerialConnectionTest::OnConnectionError, base::Unretained(this)));
    connection_->GetInfo(
        base::Bind(&SerialConnectionTest::StoreInfo, base::Unretained(this)));
    WaitForEvent(EVENT_GOT_INFO);
    ASSERT_TRUE(io_handler_.get());
  }

  void StoreInfo(serial::ConnectionInfoPtr options) {
    info_ = std::move(options);
    EventReceived(EVENT_GOT_INFO);
  }

  void StoreControlSignals(serial::DeviceControlSignalsPtr signals) {
    signals_ = std::move(signals);
    EventReceived(EVENT_GOT_CONTROL_SIGNALS);
  }

  void StoreSuccess(Event event_to_report, bool success) {
    success_ = success;
    EventReceived(event_to_report);
  }

  void Send(const base::StringPiece& data) {
    ASSERT_TRUE(sender_->Send(
        data,
        base::Bind(&SerialConnectionTest::OnDataSent, base::Unretained(this)),
        base::Bind(&SerialConnectionTest::OnSendError,
                   base::Unretained(this))));
  }

  void Receive() {
    ASSERT_TRUE(
        receiver_->Receive(base::Bind(&SerialConnectionTest::OnDataReceived,
                                      base::Unretained(this)),
                           base::Bind(&SerialConnectionTest::OnReceiveError,
                                      base::Unretained(this))));
  }

  void WaitForEvent(Event event) {
    expected_event_ = event;
    base::RunLoop run_loop;
    stop_run_loop_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void EventReceived(Event event) {
    if (event != expected_event_)
      return;
    expected_event_ = EVENT_NONE;
    ASSERT_TRUE(message_loop_);
    ASSERT_TRUE(!stop_run_loop_.is_null());
    message_loop_->PostTask(FROM_HERE, stop_run_loop_);
  }

  scoped_refptr<SerialIoHandler> CreateIoHandler() {
    io_handler_ = new TestSerialIoHandler;
    return io_handler_;
  }

  void OnDataSent(uint32_t bytes_sent) {
    bytes_sent_ += bytes_sent;
    send_error_ = serial::SendError::NONE;
    EventReceived(EVENT_DATA_SENT);
  }

  void OnSendError(uint32_t bytes_sent, int32_t error) {
    bytes_sent_ += bytes_sent;
    send_error_ = static_cast<serial::SendError>(error);
    EventReceived(EVENT_SEND_ERROR);
  }

  void OnDataReceived(std::unique_ptr<ReadOnlyBuffer> buffer) {
    data_received_ += std::string(buffer->GetData(), buffer->GetSize());
    buffer->Done(buffer->GetSize());
    receive_error_ = serial::ReceiveError::NONE;
    EventReceived(EVENT_DATA_RECEIVED);
  }

  void OnReceiveError(int32_t error) {
    receive_error_ = static_cast<serial::ReceiveError>(error);
    EventReceived(EVENT_RECEIVE_ERROR);
  }

  void OnConnectionError() {
    EventReceived(EVENT_ERROR);
    FAIL() << "Connection error";
  }

  mojo::Array<serial::DeviceInfoPtr> devices_;
  serial::ConnectionInfoPtr info_;
  serial::DeviceControlSignalsPtr signals_;
  bool connected_;
  bool success_;
  int bytes_sent_;
  serial::SendError send_error_;
  serial::ReceiveError receive_error_;
  std::string data_received_;
  Event expected_event_;

  std::unique_ptr<base::MessageLoop> message_loop_;
  base::Closure stop_run_loop_;
  mojo::InterfacePtr<serial::Connection> connection_;
  std::unique_ptr<DataSender> sender_;
  scoped_refptr<DataReceiver> receiver_;
  scoped_refptr<TestSerialIoHandler> io_handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SerialConnectionTest);
};

const uint32_t SerialConnectionTest::kBufferSize = 10;

TEST_F(SerialConnectionTest, GetInfo) {
  // |info_| is filled in during SetUp().
  ASSERT_TRUE(info_);
  EXPECT_EQ(9600u, info_->bitrate);
  EXPECT_EQ(serial::DataBits::EIGHT, info_->data_bits);
  EXPECT_EQ(serial::ParityBit::NO, info_->parity_bit);
  EXPECT_EQ(serial::StopBits::ONE, info_->stop_bits);
  EXPECT_FALSE(info_->cts_flow_control);
}

TEST_F(SerialConnectionTest, SetOptions) {
  serial::ConnectionOptionsPtr options(serial::ConnectionOptions::New());
  options->bitrate = 12345;
  options->data_bits = serial::DataBits::SEVEN;
  options->has_cts_flow_control = true;
  options->cts_flow_control = true;
  connection_->SetOptions(
      std::move(options),
      base::Bind(&SerialConnectionTest::StoreSuccess, base::Unretained(this),
                 EVENT_SET_OPTIONS));
  WaitForEvent(EVENT_SET_OPTIONS);
  ASSERT_TRUE(success_);
  serial::ConnectionInfo* info = io_handler_->connection_info();
  EXPECT_EQ(12345u, info->bitrate);
  EXPECT_EQ(serial::DataBits::SEVEN, info->data_bits);
  EXPECT_EQ(serial::ParityBit::NO, info->parity_bit);
  EXPECT_EQ(serial::StopBits::ONE, info->stop_bits);
  EXPECT_TRUE(info->cts_flow_control);
}

TEST_F(SerialConnectionTest, GetControlSignals) {
  connection_->GetControlSignals(base::Bind(
      &SerialConnectionTest::StoreControlSignals, base::Unretained(this)));
  serial::DeviceControlSignals* signals = io_handler_->device_control_signals();
  signals->dcd = true;
  signals->dsr = true;

  WaitForEvent(EVENT_GOT_CONTROL_SIGNALS);
  ASSERT_TRUE(signals_);
  EXPECT_TRUE(signals_->dcd);
  EXPECT_FALSE(signals_->cts);
  EXPECT_FALSE(signals_->ri);
  EXPECT_TRUE(signals_->dsr);
}

TEST_F(SerialConnectionTest, SetControlSignals) {
  serial::HostControlSignalsPtr signals(serial::HostControlSignals::New());
  signals->has_dtr = true;
  signals->dtr = true;
  signals->has_rts = true;
  signals->rts = true;

  connection_->SetControlSignals(
      std::move(signals),
      base::Bind(&SerialConnectionTest::StoreSuccess, base::Unretained(this),
                 EVENT_SET_CONTROL_SIGNALS));
  WaitForEvent(EVENT_SET_CONTROL_SIGNALS);
  ASSERT_TRUE(success_);
  EXPECT_TRUE(io_handler_->dtr());
  EXPECT_TRUE(io_handler_->rts());
}

TEST_F(SerialConnectionTest, Flush) {
  ASSERT_EQ(0, io_handler_->flushes());
  connection_->Flush(base::Bind(&SerialConnectionTest::StoreSuccess,
                                base::Unretained(this),
                                EVENT_FLUSHED));
  WaitForEvent(EVENT_FLUSHED);
  ASSERT_TRUE(success_);
  EXPECT_EQ(1, io_handler_->flushes());
}

TEST_F(SerialConnectionTest, DisconnectWithSend) {
  connection_.reset();
  io_handler_->set_send_callback(base::Bind(base::DoNothing));
  ASSERT_NO_FATAL_FAILURE(Send("data"));
  WaitForEvent(EVENT_SEND_ERROR);
  EXPECT_EQ(serial::SendError::DISCONNECTED, send_error_);
  EXPECT_EQ(0, bytes_sent_);
  EXPECT_TRUE(io_handler_->HasOneRef());
}

TEST_F(SerialConnectionTest, DisconnectWithReceive) {
  connection_.reset();
  ASSERT_NO_FATAL_FAILURE(Receive());
  WaitForEvent(EVENT_RECEIVE_ERROR);
  EXPECT_EQ(serial::ReceiveError::DISCONNECTED, receive_error_);
  EXPECT_EQ("", data_received_);
  EXPECT_TRUE(io_handler_->HasOneRef());
}

TEST_F(SerialConnectionTest, Echo) {
  ASSERT_NO_FATAL_FAILURE(Send("data"));
  WaitForEvent(EVENT_DATA_SENT);
  EXPECT_EQ(serial::SendError::NONE, send_error_);
  EXPECT_EQ(4, bytes_sent_);
  ASSERT_NO_FATAL_FAILURE(Receive());
  WaitForEvent(EVENT_DATA_RECEIVED);
  EXPECT_EQ("data", data_received_);
  EXPECT_EQ(serial::ReceiveError::NONE, receive_error_);
}

TEST_F(SerialConnectionTest, Cancel) {
  // To test that cancels are correctly passed to the IoHandler, we need a send
  // to be in progress because otherwise, the DataSinkReceiver would handle the
  // cancel internally.
  io_handler_->set_send_callback(
      base::Bind(&SerialConnectionTest::EventReceived,
                 base::Unretained(this),
                 EVENT_DATA_AT_IO_HANDLER));
  ASSERT_NO_FATAL_FAILURE(Send("something else"));
  WaitForEvent(EVENT_DATA_AT_IO_HANDLER);
  EXPECT_EQ(0, bytes_sent_);

  ASSERT_TRUE(sender_->Cancel(
      static_cast<int32_t>(serial::SendError::TIMEOUT),
      base::Bind(&SerialConnectionTest::EventReceived, base::Unretained(this),
                 EVENT_CANCEL_COMPLETE)));

  WaitForEvent(EVENT_CANCEL_COMPLETE);
  EXPECT_EQ(serial::SendError::TIMEOUT, send_error_);

  ASSERT_NO_FATAL_FAILURE(Send("data"));
  WaitForEvent(EVENT_DATA_SENT);
  EXPECT_EQ(serial::SendError::NONE, send_error_);
  EXPECT_EQ(4, bytes_sent_);
  ASSERT_NO_FATAL_FAILURE(Receive());
  WaitForEvent(EVENT_DATA_RECEIVED);
  EXPECT_EQ("data", data_received_);
  EXPECT_EQ(serial::ReceiveError::NONE, receive_error_);
}

}  // namespace device
