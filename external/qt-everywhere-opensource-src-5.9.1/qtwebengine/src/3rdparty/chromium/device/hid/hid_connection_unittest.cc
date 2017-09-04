// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_connection.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_io_thread.h"
#include "device/hid/hid_service.h"
#include "device/test/test_device_client.h"
#include "device/test/usb_test_gadget.h"
#include "device/usb/usb_device.h"
#include "net/base/io_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

using net::IOBufferWithSize;

// Helper class that can be used to block until a HID device with a particular
// serial number is available. Example usage:
//
//   DeviceCatcher device_catcher("ABC123");
//   HidDeviceId device_id = device_catcher.WaitForDevice();
//   /* Call HidService::Connect(device_id) to open the device. */
//
class DeviceCatcher : HidService::Observer {
 public:
  DeviceCatcher(HidService* hid_service, const base::string16& serial_number)
      : serial_number_(base::UTF16ToUTF8(serial_number)), observer_(this) {
    observer_.Add(hid_service);
    hid_service->GetDevices(base::Bind(&DeviceCatcher::OnEnumerationComplete,
                                       base::Unretained(this)));
  }

  const HidDeviceId& WaitForDevice() {
    run_loop_.Run();
    observer_.RemoveAll();
    return device_id_;
  }

 private:
  void OnEnumerationComplete(
      const std::vector<scoped_refptr<HidDeviceInfo>>& devices) {
    for (const scoped_refptr<HidDeviceInfo>& device_info : devices) {
      if (device_info->serial_number() == serial_number_) {
        device_id_ = device_info->device_id();
        run_loop_.Quit();
        break;
      }
    }
  }

  void OnDeviceAdded(scoped_refptr<HidDeviceInfo> device_info) override {
    if (device_info->serial_number() == serial_number_) {
      device_id_ = device_info->device_id();
      run_loop_.Quit();
    }
  }

  std::string serial_number_;
  ScopedObserver<device::HidService, device::HidService::Observer> observer_;
  base::RunLoop run_loop_;
  HidDeviceId device_id_;
};

class TestConnectCallback {
 public:
  TestConnectCallback()
      : callback_(base::Bind(&TestConnectCallback::SetConnection,
                             base::Unretained(this))) {}
  ~TestConnectCallback() {}

  void SetConnection(scoped_refptr<HidConnection> connection) {
    connection_ = connection;
    run_loop_.Quit();
  }

  scoped_refptr<HidConnection> WaitForConnection() {
    run_loop_.Run();
    return connection_;
  }

  const HidService::ConnectCallback& callback() { return callback_; }

 private:
  HidService::ConnectCallback callback_;
  base::RunLoop run_loop_;
  scoped_refptr<HidConnection> connection_;
};

class TestIoCallback {
 public:
  TestIoCallback()
      : read_callback_(
            base::Bind(&TestIoCallback::SetReadResult, base::Unretained(this))),
        write_callback_(base::Bind(&TestIoCallback::SetWriteResult,
                                   base::Unretained(this))) {}
  ~TestIoCallback() {}

  void SetReadResult(bool success,
                     scoped_refptr<net::IOBuffer> buffer,
                     size_t size) {
    result_ = success;
    buffer_ = buffer;
    size_ = size;
    run_loop_.Quit();
  }

  void SetWriteResult(bool success) {
    result_ = success;
    run_loop_.Quit();
  }

  bool WaitForResult() {
    run_loop_.Run();
    return result_;
  }

  const HidConnection::ReadCallback& read_callback() { return read_callback_; }
  const HidConnection::WriteCallback write_callback() {
    return write_callback_;
  }
  scoped_refptr<net::IOBuffer> buffer() const { return buffer_; }
  size_t size() const { return size_; }

 private:
  base::RunLoop run_loop_;
  bool result_;
  size_t size_;
  scoped_refptr<net::IOBuffer> buffer_;
  HidConnection::ReadCallback read_callback_;
  HidConnection::WriteCallback write_callback_;
};

}  // namespace

class HidConnectionTest : public testing::Test {
 protected:
  void SetUp() override {
    if (!UsbTestGadget::IsTestEnabled()) return;

    message_loop_.reset(new base::MessageLoopForUI());
    io_thread_.reset(new base::TestIOThread(base::TestIOThread::kAutoStart));
    device_client_.reset(new TestDeviceClient(io_thread_->task_runner()));

    service_ = DeviceClient::Get()->GetHidService();
    ASSERT_TRUE(service_);

    test_gadget_ = UsbTestGadget::Claim(io_thread_->task_runner());
    ASSERT_TRUE(test_gadget_);
    ASSERT_TRUE(test_gadget_->SetType(UsbTestGadget::HID_ECHO));

    DeviceCatcher device_catcher(service_,
                                 test_gadget_->GetDevice()->serial_number());
    device_id_ = device_catcher.WaitForDevice();
    ASSERT_NE(device_id_, kInvalidHidDeviceId);
  }

  std::unique_ptr<base::MessageLoopForUI> message_loop_;
  std::unique_ptr<base::TestIOThread> io_thread_;
  std::unique_ptr<TestDeviceClient> device_client_;
  HidService* service_;
  std::unique_ptr<UsbTestGadget> test_gadget_;
  HidDeviceId device_id_;
};

TEST_F(HidConnectionTest, ReadWrite) {
  if (!UsbTestGadget::IsTestEnabled()) return;

  TestConnectCallback connect_callback;
  service_->Connect(device_id_, connect_callback.callback());
  scoped_refptr<HidConnection> conn = connect_callback.WaitForConnection();
  ASSERT_TRUE(conn.get());

  const char kBufferSize = 9;
  for (char i = 0; i < 8; ++i) {
    scoped_refptr<IOBufferWithSize> buffer(new IOBufferWithSize(kBufferSize));
    buffer->data()[0] = 0;
    for (unsigned char j = 1; j < kBufferSize; ++j) {
      buffer->data()[j] = i + j - 1;
    }

    TestIoCallback write_callback;
    conn->Write(buffer, buffer->size(), write_callback.write_callback());
    ASSERT_TRUE(write_callback.WaitForResult());

    TestIoCallback read_callback;
    conn->Read(read_callback.read_callback());
    ASSERT_TRUE(read_callback.WaitForResult());
    ASSERT_EQ(9UL, read_callback.size());
    ASSERT_EQ(0, read_callback.buffer()->data()[0]);
    for (unsigned char j = 1; j < kBufferSize; ++j) {
      ASSERT_EQ(i + j - 1, read_callback.buffer()->data()[j]);
    }
  }

  conn->Close();
}

}  // namespace device
