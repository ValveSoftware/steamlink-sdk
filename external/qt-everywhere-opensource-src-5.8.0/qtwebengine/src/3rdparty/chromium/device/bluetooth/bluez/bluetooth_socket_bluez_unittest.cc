// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/bluetooth_socket_bluez.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/bluetooth_socket_thread.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "device/bluetooth/bluez/bluetooth_adapter_bluez.h"
#include "device/bluetooth/bluez/bluetooth_device_bluez.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_agent_manager_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_device_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_service_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_input_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_profile_manager_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_profile_service_provider.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

using device::BluetoothAdapter;
using device::BluetoothDevice;
using device::BluetoothSocket;
using device::BluetoothSocketThread;
using device::BluetoothUUID;

namespace {

void DoNothingDBusErrorCallback(const std::string& error_name,
                                const std::string& error_message) {}

}  // namespace

namespace bluez {

class BluetoothSocketBlueZTest : public testing::Test {
 public:
  BluetoothSocketBlueZTest()
      : success_callback_count_(0),
        error_callback_count_(0),
        last_bytes_sent_(0),
        last_bytes_received_(0),
        last_reason_(BluetoothSocket::kSystemError) {}

  void SetUp() override {
    std::unique_ptr<bluez::BluezDBusManagerSetter> dbus_setter =
        bluez::BluezDBusManager::GetSetterForTesting();

    dbus_setter->SetBluetoothAdapterClient(
        std::unique_ptr<bluez::BluetoothAdapterClient>(
            new bluez::FakeBluetoothAdapterClient));
    dbus_setter->SetBluetoothAgentManagerClient(
        std::unique_ptr<bluez::BluetoothAgentManagerClient>(
            new bluez::FakeBluetoothAgentManagerClient));
    dbus_setter->SetBluetoothDeviceClient(
        std::unique_ptr<bluez::BluetoothDeviceClient>(
            new bluez::FakeBluetoothDeviceClient));
    dbus_setter->SetBluetoothGattServiceClient(
        std::unique_ptr<bluez::BluetoothGattServiceClient>(
            new bluez::FakeBluetoothGattServiceClient));
    dbus_setter->SetBluetoothInputClient(
        std::unique_ptr<bluez::BluetoothInputClient>(
            new bluez::FakeBluetoothInputClient));
    dbus_setter->SetBluetoothProfileManagerClient(
        std::unique_ptr<bluez::BluetoothProfileManagerClient>(
            new bluez::FakeBluetoothProfileManagerClient));

    BluetoothSocketThread::Get();

    // Grab a pointer to the adapter.
    device::BluetoothAdapterFactory::GetAdapter(base::Bind(
        &BluetoothSocketBlueZTest::AdapterCallback, base::Unretained(this)));

    base::MessageLoop::current()->Run();

    ASSERT_TRUE(adapter_.get() != nullptr);
    ASSERT_TRUE(adapter_->IsInitialized());
    ASSERT_TRUE(adapter_->IsPresent());

    // Turn on the adapter.
    adapter_->SetPowered(true, base::Bind(&base::DoNothing),
                         base::Bind(&base::DoNothing));
    ASSERT_TRUE(adapter_->IsPowered());
  }

  void TearDown() override {
    adapter_ = nullptr;
    BluetoothSocketThread::CleanupForTesting();
    bluez::BluezDBusManager::Shutdown();
  }

  void AdapterCallback(scoped_refptr<BluetoothAdapter> adapter) {
    adapter_ = adapter;
    if (base::MessageLoop::current() &&
        base::MessageLoop::current()->is_running()) {
      base::MessageLoop::current()->QuitWhenIdle();
    }
  }

  void SuccessCallback() {
    ++success_callback_count_;
    message_loop_.QuitWhenIdle();
  }

  void ErrorCallback(const std::string& message) {
    ++error_callback_count_;
    last_message_ = message;

    if (message_loop_.is_running())
      message_loop_.QuitWhenIdle();
  }

  void ConnectToServiceSuccessCallback(scoped_refptr<BluetoothSocket> socket) {
    ++success_callback_count_;
    last_socket_ = socket;

    message_loop_.QuitWhenIdle();
  }

  void SendSuccessCallback(int bytes_sent) {
    ++success_callback_count_;
    last_bytes_sent_ = bytes_sent;

    message_loop_.QuitWhenIdle();
  }

  void ReceiveSuccessCallback(int bytes_received,
                              scoped_refptr<net::IOBuffer> io_buffer) {
    ++success_callback_count_;
    last_bytes_received_ = bytes_received;
    last_io_buffer_ = io_buffer;

    message_loop_.QuitWhenIdle();
  }

  void ReceiveErrorCallback(BluetoothSocket::ErrorReason reason,
                            const std::string& error_message) {
    ++error_callback_count_;
    last_reason_ = reason;
    last_message_ = error_message;

    message_loop_.QuitWhenIdle();
  }

  void CreateServiceSuccessCallback(scoped_refptr<BluetoothSocket> socket) {
    ++success_callback_count_;
    last_socket_ = socket;

    if (message_loop_.is_running())
      message_loop_.QuitWhenIdle();
  }

  void AcceptSuccessCallback(const BluetoothDevice* device,
                             scoped_refptr<BluetoothSocket> socket) {
    ++success_callback_count_;
    last_device_ = device;
    last_socket_ = socket;

    message_loop_.QuitWhenIdle();
  }

  void ImmediateSuccessCallback() { ++success_callback_count_; }

 protected:
  base::MessageLoop message_loop_;

  scoped_refptr<BluetoothAdapter> adapter_;

  unsigned int success_callback_count_;
  unsigned int error_callback_count_;

  std::string last_message_;
  scoped_refptr<BluetoothSocket> last_socket_;
  int last_bytes_sent_;
  int last_bytes_received_;
  scoped_refptr<net::IOBuffer> last_io_buffer_;
  BluetoothSocket::ErrorReason last_reason_;
  const BluetoothDevice* last_device_;
};

TEST_F(BluetoothSocketBlueZTest, Connect) {
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_TRUE(device != nullptr);

  device->ConnectToService(
      BluetoothUUID(bluez::FakeBluetoothProfileManagerClient::kRfcommUuid),
      base::Bind(&BluetoothSocketBlueZTest::ConnectToServiceSuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothSocketBlueZTest::ErrorCallback,
                 base::Unretained(this)));
  message_loop_.Run();

  EXPECT_EQ(1U, success_callback_count_);
  EXPECT_EQ(0U, error_callback_count_);
  EXPECT_TRUE(last_socket_.get() != nullptr);

  // Take ownership of the socket for the remainder of the test.
  scoped_refptr<BluetoothSocket> socket = last_socket_;
  last_socket_ = nullptr;
  success_callback_count_ = 0;
  error_callback_count_ = 0;

  // Send data to the socket, expect all of the data to be sent.
  scoped_refptr<net::StringIOBuffer> write_buffer(
      new net::StringIOBuffer("test"));

  socket->Send(write_buffer.get(), write_buffer->size(),
               base::Bind(&BluetoothSocketBlueZTest::SendSuccessCallback,
                          base::Unretained(this)),
               base::Bind(&BluetoothSocketBlueZTest::ErrorCallback,
                          base::Unretained(this)));
  message_loop_.Run();

  EXPECT_EQ(1U, success_callback_count_);
  EXPECT_EQ(0U, error_callback_count_);
  EXPECT_EQ(last_bytes_sent_, write_buffer->size());

  success_callback_count_ = 0;
  error_callback_count_ = 0;

  // Receive data from the socket, and fetch the buffer from the callback; since
  // the fake is an echo server, we expect to receive what we wrote.
  socket->Receive(4096,
                  base::Bind(&BluetoothSocketBlueZTest::ReceiveSuccessCallback,
                             base::Unretained(this)),
                  base::Bind(&BluetoothSocketBlueZTest::ReceiveErrorCallback,
                             base::Unretained(this)));
  message_loop_.Run();

  EXPECT_EQ(1U, success_callback_count_);
  EXPECT_EQ(0U, error_callback_count_);
  EXPECT_EQ(4, last_bytes_received_);
  EXPECT_TRUE(last_io_buffer_.get() != nullptr);

  // Take ownership of the received buffer.
  scoped_refptr<net::IOBuffer> read_buffer = last_io_buffer_;
  last_io_buffer_ = nullptr;
  success_callback_count_ = 0;
  error_callback_count_ = 0;

  std::string data = std::string(read_buffer->data(), last_bytes_received_);
  EXPECT_EQ("test", data);

  read_buffer = nullptr;

  // Receive data again; the socket will have been closed, this should cause a
  // disconnected error to be returned via the error callback.
  socket->Receive(4096,
                  base::Bind(&BluetoothSocketBlueZTest::ReceiveSuccessCallback,
                             base::Unretained(this)),
                  base::Bind(&BluetoothSocketBlueZTest::ReceiveErrorCallback,
                             base::Unretained(this)));
  message_loop_.Run();

  EXPECT_EQ(0U, success_callback_count_);
  EXPECT_EQ(1U, error_callback_count_);
  EXPECT_EQ(BluetoothSocket::kDisconnected, last_reason_);
  EXPECT_EQ(net::ErrorToString(net::OK), last_message_);

  success_callback_count_ = 0;
  error_callback_count_ = 0;

  // Send data again; since the socket is closed we should get a system error
  // equivalent to the connection reset error.
  write_buffer = new net::StringIOBuffer("second test");

  socket->Send(write_buffer.get(), write_buffer->size(),
               base::Bind(&BluetoothSocketBlueZTest::SendSuccessCallback,
                          base::Unretained(this)),
               base::Bind(&BluetoothSocketBlueZTest::ErrorCallback,
                          base::Unretained(this)));
  message_loop_.Run();

  EXPECT_EQ(0U, success_callback_count_);
  EXPECT_EQ(1U, error_callback_count_);
  EXPECT_EQ(net::ErrorToString(net::ERR_CONNECTION_RESET), last_message_);

  success_callback_count_ = 0;
  error_callback_count_ = 0;

  // Close our end of the socket.
  socket->Disconnect(base::Bind(&BluetoothSocketBlueZTest::SuccessCallback,
                                base::Unretained(this)));

  message_loop_.Run();
  EXPECT_EQ(1U, success_callback_count_);
}

TEST_F(BluetoothSocketBlueZTest, Listen) {
  adapter_->CreateRfcommService(
      BluetoothUUID(bluez::FakeBluetoothProfileManagerClient::kRfcommUuid),
      BluetoothAdapter::ServiceOptions(),
      base::Bind(&BluetoothSocketBlueZTest::CreateServiceSuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothSocketBlueZTest::ErrorCallback,
                 base::Unretained(this)));

  message_loop_.Run();

  EXPECT_EQ(1U, success_callback_count_);
  EXPECT_EQ(0U, error_callback_count_);
  EXPECT_TRUE(last_socket_.get() != nullptr);

  // Take ownership of the socket for the remainder of the test.
  scoped_refptr<BluetoothSocket> server_socket = last_socket_;
  last_socket_ = nullptr;
  success_callback_count_ = 0;
  error_callback_count_ = 0;

  // Simulate an incoming connection by just calling the ConnectProfile method
  // of the underlying fake device client (from the BlueZ point of view,
  // outgoing and incoming look the same).
  //
  // This is done before the Accept() call to simulate a pending call at the
  // point that Accept() is called.
  bluez::FakeBluetoothDeviceClient* fake_bluetooth_device_client =
      static_cast<bluez::FakeBluetoothDeviceClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothDeviceClient());
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_TRUE(device != nullptr);
  fake_bluetooth_device_client->ConnectProfile(
      static_cast<BluetoothDeviceBlueZ*>(device)->object_path(),
      bluez::FakeBluetoothProfileManagerClient::kRfcommUuid,
      base::Bind(&base::DoNothing), base::Bind(&DoNothingDBusErrorCallback));

  message_loop_.RunUntilIdle();

  server_socket->Accept(
      base::Bind(&BluetoothSocketBlueZTest::AcceptSuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothSocketBlueZTest::ErrorCallback,
                 base::Unretained(this)));

  message_loop_.Run();

  EXPECT_EQ(1U, success_callback_count_);
  EXPECT_EQ(0U, error_callback_count_);
  EXPECT_TRUE(last_socket_.get() != nullptr);

  // Take ownership of the client socket for the remainder of the test.
  scoped_refptr<BluetoothSocket> client_socket = last_socket_;
  last_socket_ = nullptr;
  success_callback_count_ = 0;
  error_callback_count_ = 0;

  // Close our end of the client socket.
  client_socket->Disconnect(base::Bind(
      &BluetoothSocketBlueZTest::SuccessCallback, base::Unretained(this)));

  message_loop_.Run();

  EXPECT_EQ(1U, success_callback_count_);
  client_socket = nullptr;
  success_callback_count_ = 0;
  error_callback_count_ = 0;

  // Run a second connection test, this time calling Accept() before the
  // incoming connection comes in.
  server_socket->Accept(
      base::Bind(&BluetoothSocketBlueZTest::AcceptSuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothSocketBlueZTest::ErrorCallback,
                 base::Unretained(this)));

  message_loop_.RunUntilIdle();

  fake_bluetooth_device_client->ConnectProfile(
      static_cast<BluetoothDeviceBlueZ*>(device)->object_path(),
      bluez::FakeBluetoothProfileManagerClient::kRfcommUuid,
      base::Bind(&base::DoNothing), base::Bind(&DoNothingDBusErrorCallback));

  message_loop_.Run();

  EXPECT_EQ(1U, success_callback_count_);
  EXPECT_EQ(0U, error_callback_count_);
  EXPECT_TRUE(last_socket_.get() != nullptr);

  // Take ownership of the client socket for the remainder of the test.
  client_socket = last_socket_;
  last_socket_ = nullptr;
  success_callback_count_ = 0;
  error_callback_count_ = 0;

  // Close our end of the client socket.
  client_socket->Disconnect(base::Bind(
      &BluetoothSocketBlueZTest::SuccessCallback, base::Unretained(this)));

  message_loop_.Run();

  EXPECT_EQ(1U, success_callback_count_);
  client_socket = nullptr;
  success_callback_count_ = 0;
  error_callback_count_ = 0;

  // Now close the server socket.
  server_socket->Disconnect(
      base::Bind(&BluetoothSocketBlueZTest::ImmediateSuccessCallback,
                 base::Unretained(this)));

  message_loop_.RunUntilIdle();

  EXPECT_EQ(1U, success_callback_count_);
}

TEST_F(BluetoothSocketBlueZTest, ListenBeforeAdapterStart) {
  // Start off with an invisible adapter, register the profile, then make
  // the adapter visible.
  bluez::FakeBluetoothAdapterClient* fake_bluetooth_adapter_client =
      static_cast<bluez::FakeBluetoothAdapterClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothAdapterClient());
  fake_bluetooth_adapter_client->SetVisible(false);

  adapter_->CreateRfcommService(
      BluetoothUUID(bluez::FakeBluetoothProfileManagerClient::kRfcommUuid),
      BluetoothAdapter::ServiceOptions(),
      base::Bind(&BluetoothSocketBlueZTest::CreateServiceSuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothSocketBlueZTest::ErrorCallback,
                 base::Unretained(this)));
  message_loop_.Run();

  EXPECT_EQ(1U, success_callback_count_);
  EXPECT_EQ(0U, error_callback_count_);
  EXPECT_TRUE(last_socket_.get() != nullptr);

  // Take ownership of the socket for the remainder of the test.
  scoped_refptr<BluetoothSocket> socket = last_socket_;
  last_socket_ = nullptr;
  success_callback_count_ = 0;
  error_callback_count_ = 0;

  // But there shouldn't be a profile registered yet.
  bluez::FakeBluetoothProfileManagerClient*
      fake_bluetooth_profile_manager_client =
          static_cast<bluez::FakeBluetoothProfileManagerClient*>(
              bluez::BluezDBusManager::Get()
                  ->GetBluetoothProfileManagerClient());
  bluez::FakeBluetoothProfileServiceProvider* profile_service_provider =
      fake_bluetooth_profile_manager_client->GetProfileServiceProvider(
          bluez::FakeBluetoothProfileManagerClient::kRfcommUuid);
  EXPECT_TRUE(profile_service_provider == nullptr);

  // Make the adapter visible. This should register a profile.
  fake_bluetooth_adapter_client->SetVisible(true);

  message_loop_.RunUntilIdle();

  profile_service_provider =
      fake_bluetooth_profile_manager_client->GetProfileServiceProvider(
          bluez::FakeBluetoothProfileManagerClient::kRfcommUuid);
  EXPECT_TRUE(profile_service_provider != nullptr);

  // Cleanup the socket.
  socket->Disconnect(
      base::Bind(&BluetoothSocketBlueZTest::ImmediateSuccessCallback,
                 base::Unretained(this)));

  message_loop_.RunUntilIdle();

  EXPECT_EQ(1U, success_callback_count_);
}

TEST_F(BluetoothSocketBlueZTest, ListenAcrossAdapterRestart) {
  // The fake adapter starts off visible by default.
  bluez::FakeBluetoothAdapterClient* fake_bluetooth_adapter_client =
      static_cast<bluez::FakeBluetoothAdapterClient*>(
          bluez::BluezDBusManager::Get()->GetBluetoothAdapterClient());

  adapter_->CreateRfcommService(
      BluetoothUUID(bluez::FakeBluetoothProfileManagerClient::kRfcommUuid),
      BluetoothAdapter::ServiceOptions(),
      base::Bind(&BluetoothSocketBlueZTest::CreateServiceSuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothSocketBlueZTest::ErrorCallback,
                 base::Unretained(this)));
  message_loop_.Run();

  EXPECT_EQ(1U, success_callback_count_);
  EXPECT_EQ(0U, error_callback_count_);
  EXPECT_TRUE(last_socket_.get() != nullptr);

  // Take ownership of the socket for the remainder of the test.
  scoped_refptr<BluetoothSocket> socket = last_socket_;
  last_socket_ = nullptr;
  success_callback_count_ = 0;
  error_callback_count_ = 0;

  // Make sure the profile was registered with the daemon.
  bluez::FakeBluetoothProfileManagerClient*
      fake_bluetooth_profile_manager_client =
          static_cast<bluez::FakeBluetoothProfileManagerClient*>(
              bluez::BluezDBusManager::Get()
                  ->GetBluetoothProfileManagerClient());
  bluez::FakeBluetoothProfileServiceProvider* profile_service_provider =
      fake_bluetooth_profile_manager_client->GetProfileServiceProvider(
          bluez::FakeBluetoothProfileManagerClient::kRfcommUuid);
  EXPECT_TRUE(profile_service_provider != nullptr);

  // Make the adapter invisible, and fiddle with the profile fake to unregister
  // the profile since this doesn't happen automatically.
  fake_bluetooth_adapter_client->SetVisible(false);

  message_loop_.RunUntilIdle();

  // Then make the adapter visible again. This should re-register the profile.
  fake_bluetooth_adapter_client->SetVisible(true);

  message_loop_.RunUntilIdle();

  profile_service_provider =
      fake_bluetooth_profile_manager_client->GetProfileServiceProvider(
          bluez::FakeBluetoothProfileManagerClient::kRfcommUuid);
  EXPECT_TRUE(profile_service_provider != nullptr);

  // Cleanup the socket.
  socket->Disconnect(
      base::Bind(&BluetoothSocketBlueZTest::ImmediateSuccessCallback,
                 base::Unretained(this)));

  message_loop_.RunUntilIdle();

  EXPECT_EQ(1U, success_callback_count_);
}

TEST_F(BluetoothSocketBlueZTest, PairedConnectFails) {
  BluetoothDevice* device = adapter_->GetDevice(
      bluez::FakeBluetoothDeviceClient::kPairedUnconnectableDeviceAddress);
  ASSERT_TRUE(device != nullptr);

  device->ConnectToService(
      BluetoothUUID(bluez::FakeBluetoothProfileManagerClient::kRfcommUuid),
      base::Bind(&BluetoothSocketBlueZTest::ConnectToServiceSuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothSocketBlueZTest::ErrorCallback,
                 base::Unretained(this)));
  message_loop_.Run();

  EXPECT_EQ(0U, success_callback_count_);
  EXPECT_EQ(1U, error_callback_count_);
  EXPECT_TRUE(last_socket_.get() == nullptr);

  device->ConnectToService(
      BluetoothUUID(bluez::FakeBluetoothProfileManagerClient::kRfcommUuid),
      base::Bind(&BluetoothSocketBlueZTest::ConnectToServiceSuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothSocketBlueZTest::ErrorCallback,
                 base::Unretained(this)));
  message_loop_.Run();

  EXPECT_EQ(0U, success_callback_count_);
  EXPECT_EQ(2U, error_callback_count_);
  EXPECT_TRUE(last_socket_.get() == nullptr);
}

TEST_F(BluetoothSocketBlueZTest, SocketListenTwice) {
  adapter_->CreateRfcommService(
      BluetoothUUID(bluez::FakeBluetoothProfileManagerClient::kRfcommUuid),
      BluetoothAdapter::ServiceOptions(),
      base::Bind(&BluetoothSocketBlueZTest::CreateServiceSuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothSocketBlueZTest::ErrorCallback,
                 base::Unretained(this)));

  message_loop_.Run();

  EXPECT_EQ(1U, success_callback_count_);
  EXPECT_EQ(0U, error_callback_count_);
  EXPECT_TRUE(last_socket_.get() != nullptr);

  // Take control of this socket.
  scoped_refptr<BluetoothSocket> server_socket;
  server_socket.swap(last_socket_);

  server_socket->Accept(
      base::Bind(&BluetoothSocketBlueZTest::AcceptSuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothSocketBlueZTest::ErrorCallback,
                 base::Unretained(this)));

  server_socket->Close();

  server_socket = nullptr;

  message_loop_.RunUntilIdle();

  EXPECT_EQ(1U, success_callback_count_);
  EXPECT_EQ(1U, error_callback_count_);

  adapter_->CreateRfcommService(
      BluetoothUUID(bluez::FakeBluetoothProfileManagerClient::kRfcommUuid),
      BluetoothAdapter::ServiceOptions(),
      base::Bind(&BluetoothSocketBlueZTest::CreateServiceSuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothSocketBlueZTest::ErrorCallback,
                 base::Unretained(this)));

  message_loop_.Run();

  EXPECT_EQ(2U, success_callback_count_);
  EXPECT_EQ(1U, error_callback_count_);
  EXPECT_TRUE(last_socket_.get() != nullptr);

  // Take control of this socket.
  server_socket.swap(last_socket_);

  server_socket->Accept(
      base::Bind(&BluetoothSocketBlueZTest::AcceptSuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothSocketBlueZTest::ErrorCallback,
                 base::Unretained(this)));

  server_socket->Close();

  server_socket = nullptr;

  message_loop_.RunUntilIdle();

  EXPECT_EQ(2U, success_callback_count_);
  EXPECT_EQ(2U, error_callback_count_);
}

}  // namespace bluez
