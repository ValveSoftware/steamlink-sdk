// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "device/hid/hid_connection.h"
#include "device/hid/hid_service.h"
#include "net/base/io_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

using net::IOBufferWithSize;

const int kUSBLUFADemoVID = 0x03eb;
const int kUSBLUFADemoPID = 0x204f;
const uint64_t kReport = 0x0903a65d030f8ec9ULL;

int g_read_times = 0;
void Read(scoped_refptr<HidConnection> conn);

void OnRead(scoped_refptr<HidConnection> conn,
            scoped_refptr<IOBufferWithSize> buffer,
            bool success,
            size_t bytes) {
  EXPECT_TRUE(success);
  if (success) {
    g_read_times++;
    EXPECT_EQ(8U, bytes);
    if (bytes == 8) {
      uint64_t* data = reinterpret_cast<uint64_t*>(buffer->data());
      EXPECT_EQ(kReport, *data);
    } else {
      base::MessageLoop::current()->Quit();
    }
  } else {
    LOG(ERROR) << "~";
    g_read_times++;
  }

  if (g_read_times < 3){
    base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind(Read, conn));
  } else {
    base::MessageLoop::current()->Quit();
  }
}

void Read(scoped_refptr<HidConnection> conn) {
  scoped_refptr<IOBufferWithSize> buffer(new IOBufferWithSize(8));
  conn->Read(buffer, base::Bind(OnRead, conn, buffer));
}

void OnWriteNormal(bool success,
                   size_t bytes) {
  ASSERT_TRUE(success);
  base::MessageLoop::current()->Quit();
}

void WriteNormal(scoped_refptr<HidConnection> conn) {
  scoped_refptr<IOBufferWithSize> buffer(new IOBufferWithSize(8));
  *(int64_t*)buffer->data() = kReport;

  conn->Write(0, buffer, base::Bind(OnWriteNormal));
}

}  // namespace

class HidConnectionTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    message_loop_.reset(new base::MessageLoopForIO());
    service_.reset(HidService::CreateInstance());
    ASSERT_TRUE(service_);

    std::vector<HidDeviceInfo> devices;
    service_->GetDevices(&devices);
    device_id_ = kInvalidHidDeviceId;
    for (std::vector<HidDeviceInfo>::iterator it = devices.begin();
        it != devices.end();
        ++it) {
      if (it->vendor_id == kUSBLUFADemoVID &&
          it->product_id == kUSBLUFADemoPID) {
        device_id_ = it->device_id;
        return;
      }
    }
  }

  virtual void TearDown() OVERRIDE {
    service_.reset(NULL);
    message_loop_.reset(NULL);
  }

  HidDeviceId device_id_;
  scoped_ptr<base::MessageLoopForIO> message_loop_;
  scoped_ptr<HidService> service_;
};

TEST_F(HidConnectionTest, Create) {
  scoped_refptr<HidConnection> connection = service_->Connect(device_id_);
  ASSERT_TRUE(connection || device_id_ == kInvalidHidDeviceId);
}

TEST_F(HidConnectionTest, Read) {
  scoped_refptr<HidConnection> connection = service_->Connect(device_id_);
  if (connection) {
    message_loop_->PostTask(FROM_HERE, base::Bind(Read, connection));
    message_loop_->Run();
  }
}

TEST_F(HidConnectionTest, Write) {
  scoped_refptr<HidConnection> connection = service_->Connect(device_id_);

  if (connection) {
    message_loop_->PostTask(FROM_HERE, base::Bind(WriteNormal, connection));
    message_loop_->Run();
  }
}

}  // namespace device
