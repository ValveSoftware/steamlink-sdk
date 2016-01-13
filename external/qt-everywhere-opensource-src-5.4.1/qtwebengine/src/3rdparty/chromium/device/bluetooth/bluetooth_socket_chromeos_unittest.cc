// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "chromeos/dbus/fake_bluetooth_adapter_client.h"
#include "chromeos/dbus/fake_bluetooth_agent_manager_client.h"
#include "chromeos/dbus/fake_bluetooth_device_client.h"
#include "chromeos/dbus/fake_bluetooth_gatt_service_client.h"
#include "chromeos/dbus/fake_bluetooth_input_client.h"
#include "chromeos/dbus/fake_bluetooth_profile_manager_client.h"
#include "chromeos/dbus/fake_dbus_thread_manager.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_chromeos.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_device_chromeos.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/bluetooth_socket_chromeos.h"
#include "device/bluetooth/bluetooth_socket_thread.h"
#include "device/bluetooth/bluetooth_uuid.h"
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

namespace chromeos {

class BluetoothSocketChromeOSTest : public testing::Test {
 public:
  BluetoothSocketChromeOSTest()
      : success_callback_count_(0),
        error_callback_count_(0),
        last_bytes_sent_(0),
        last_bytes_received_(0),
        last_reason_(BluetoothSocket::kSystemError) {}

  virtual void SetUp() OVERRIDE {
    scoped_ptr<FakeDBusThreadManager> fake_dbus_thread_manager(
        new FakeDBusThreadManager);

    fake_dbus_thread_manager->SetBluetoothAdapterClient(
        scoped_ptr<BluetoothAdapterClient>(new FakeBluetoothAdapterClient));
    fake_dbus_thread_manager->SetBluetoothAgentManagerClient(
        scoped_ptr<BluetoothAgentManagerClient>(
            new FakeBluetoothAgentManagerClient));
    fake_dbus_thread_manager->SetBluetoothDeviceClient(
        scoped_ptr<BluetoothDeviceClient>(new FakeBluetoothDeviceClient));
    fake_dbus_thread_manager->SetBluetoothGattServiceClient(
        scoped_ptr<BluetoothGattServiceClient>(
            new FakeBluetoothGattServiceClient));
    fake_dbus_thread_manager->SetBluetoothInputClient(
        scoped_ptr<BluetoothInputClient>(new FakeBluetoothInputClient));
    fake_dbus_thread_manager->SetBluetoothProfileManagerClient(
        scoped_ptr<BluetoothProfileManagerClient>(
            new FakeBluetoothProfileManagerClient));

    DBusThreadManager::InitializeForTesting(fake_dbus_thread_manager.release());
    BluetoothSocketThread::Get();

    // Grab a pointer to the adapter.
    device::BluetoothAdapterFactory::GetAdapter(
        base::Bind(&BluetoothSocketChromeOSTest::AdapterCallback,
                   base::Unretained(this)));
    ASSERT_TRUE(adapter_.get() != NULL);
    ASSERT_TRUE(adapter_->IsInitialized());
    ASSERT_TRUE(adapter_->IsPresent());

    // Turn on the adapter.
    adapter_->SetPowered(
        true,
        base::Bind(&base::DoNothing),
        base::Bind(&base::DoNothing));
    ASSERT_TRUE(adapter_->IsPowered());
  }

  virtual void TearDown() OVERRIDE {
    adapter_ = NULL;
    BluetoothSocketThread::CleanupForTesting();
    DBusThreadManager::Shutdown();
  }

  void AdapterCallback(scoped_refptr<BluetoothAdapter> adapter) {
    adapter_ = adapter;
  }

  void SuccessCallback() {
    ++success_callback_count_;
    message_loop_.Quit();
  }

  void ErrorCallback(const std::string& message) {
    ++error_callback_count_;
    last_message_ = message;

    message_loop_.Quit();
  }

  void ConnectToServiceSuccessCallback(scoped_refptr<BluetoothSocket> socket) {
    ++success_callback_count_;
    last_socket_ = socket;

    message_loop_.Quit();
  }

  void SendSuccessCallback(int bytes_sent) {
    ++success_callback_count_;
    last_bytes_sent_ = bytes_sent;

    message_loop_.Quit();
  }

  void ReceiveSuccessCallback(int bytes_received,
                              scoped_refptr<net::IOBuffer> io_buffer) {
    ++success_callback_count_;
    last_bytes_received_ = bytes_received;
    last_io_buffer_ = io_buffer;

    message_loop_.Quit();
  }

  void ReceiveErrorCallback(BluetoothSocket::ErrorReason reason,
                            const std::string& error_message) {
    ++error_callback_count_;
    last_reason_ = reason;
    last_message_ = error_message;

    message_loop_.Quit();
  }

  void CreateServiceSuccessCallback(scoped_refptr<BluetoothSocket> socket) {
    ++success_callback_count_;
    last_socket_ = socket;
  }

  void AcceptSuccessCallback(const BluetoothDevice* device,
                             scoped_refptr<BluetoothSocket> socket) {
    ++success_callback_count_;
    last_device_ = device;
    last_socket_ = socket;

    message_loop_.Quit();
  }

  void ImmediateSuccessCallback() {
    ++success_callback_count_;
  }

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

TEST_F(BluetoothSocketChromeOSTest, Connect) {
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_TRUE(device != NULL);

  device->ConnectToService(
      BluetoothUUID(FakeBluetoothProfileManagerClient::kRfcommUuid),
      base::Bind(&BluetoothSocketChromeOSTest::ConnectToServiceSuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothSocketChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  message_loop_.Run();

  EXPECT_EQ(1U, success_callback_count_);
  EXPECT_EQ(0U, error_callback_count_);
  EXPECT_TRUE(last_socket_.get() != NULL);

  // Take ownership of the socket for the remainder of the test.
  scoped_refptr<BluetoothSocket> socket = last_socket_;
  last_socket_ = NULL;
  success_callback_count_ = 0;
  error_callback_count_ = 0;

  // Send data to the socket, expect all of the data to be sent.
  scoped_refptr<net::StringIOBuffer> write_buffer(
      new net::StringIOBuffer("test"));

  socket->Send(write_buffer.get(), write_buffer->size(),
               base::Bind(&BluetoothSocketChromeOSTest::SendSuccessCallback,
                          base::Unretained(this)),
               base::Bind(&BluetoothSocketChromeOSTest::ErrorCallback,
                          base::Unretained(this)));
  message_loop_.Run();

  EXPECT_EQ(1U, success_callback_count_);
  EXPECT_EQ(0U, error_callback_count_);
  EXPECT_EQ(last_bytes_sent_, write_buffer->size());

  success_callback_count_ = 0;
  error_callback_count_ = 0;

  // Receive data from the socket, and fetch the buffer from the callback; since
  // the fake is an echo server, we expect to receive what we wrote.
  socket->Receive(
      4096,
      base::Bind(&BluetoothSocketChromeOSTest::ReceiveSuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothSocketChromeOSTest::ReceiveErrorCallback,
                 base::Unretained(this)));
  message_loop_.Run();

  EXPECT_EQ(1U, success_callback_count_);
  EXPECT_EQ(0U, error_callback_count_);
  EXPECT_EQ(4, last_bytes_received_);
  EXPECT_TRUE(last_io_buffer_.get() != NULL);

  // Take ownership of the received buffer.
  scoped_refptr<net::IOBuffer> read_buffer = last_io_buffer_;
  last_io_buffer_ = NULL;
  success_callback_count_ = 0;
  error_callback_count_ = 0;

  std::string data = std::string(read_buffer->data(), last_bytes_received_);
  EXPECT_EQ("test", data);

  read_buffer = NULL;

  // Receive data again; the socket will have been closed, this should cause a
  // disconnected error to be returned via the error callback.
  socket->Receive(
      4096,
      base::Bind(&BluetoothSocketChromeOSTest::ReceiveSuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothSocketChromeOSTest::ReceiveErrorCallback,
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
               base::Bind(&BluetoothSocketChromeOSTest::SendSuccessCallback,
                          base::Unretained(this)),
               base::Bind(&BluetoothSocketChromeOSTest::ErrorCallback,
                          base::Unretained(this)));
  message_loop_.Run();

  EXPECT_EQ(0U, success_callback_count_);
  EXPECT_EQ(1U, error_callback_count_);
  EXPECT_EQ(net::ErrorToString(net::ERR_CONNECTION_RESET), last_message_);

  success_callback_count_ = 0;
  error_callback_count_ = 0;

  // Close our end of the socket.
  socket->Disconnect(base::Bind(&BluetoothSocketChromeOSTest::SuccessCallback,
                                base::Unretained(this)));

  message_loop_.Run();
  EXPECT_EQ(1U, success_callback_count_);
}

TEST_F(BluetoothSocketChromeOSTest, Listen) {
  adapter_->CreateRfcommService(
      BluetoothUUID(FakeBluetoothProfileManagerClient::kRfcommUuid),
      BluetoothAdapter::kChannelAuto,
      base::Bind(&BluetoothSocketChromeOSTest::CreateServiceSuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothSocketChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1U, success_callback_count_);
  EXPECT_EQ(0U, error_callback_count_);
  EXPECT_TRUE(last_socket_.get() != NULL);

  // Take ownership of the socket for the remainder of the test.
  scoped_refptr<BluetoothSocket> server_socket = last_socket_;
  last_socket_ = NULL;
  success_callback_count_ = 0;
  error_callback_count_ = 0;

  // Simulate an incoming connection by just calling the ConnectProfile method
  // of the underlying fake device client (from the BlueZ point of view,
  // outgoing and incoming look the same).
  //
  // This is done before the Accept() call to simulate a pending call at the
  // point that Accept() is called.
  FakeBluetoothDeviceClient* fake_bluetooth_device_client =
      static_cast<FakeBluetoothDeviceClient*>(
          DBusThreadManager::Get()->GetBluetoothDeviceClient());
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_TRUE(device != NULL);
  fake_bluetooth_device_client->ConnectProfile(
      static_cast<BluetoothDeviceChromeOS*>(device)->object_path(),
      FakeBluetoothProfileManagerClient::kRfcommUuid,
      base::Bind(&base::DoNothing),
      base::Bind(&DoNothingDBusErrorCallback));

  server_socket->Accept(
      base::Bind(&BluetoothSocketChromeOSTest::AcceptSuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothSocketChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  message_loop_.Run();

  EXPECT_EQ(1U, success_callback_count_);
  EXPECT_EQ(0U, error_callback_count_);
  EXPECT_TRUE(last_socket_.get() != NULL);

  // Take ownership of the client socket for the remainder of the test.
  scoped_refptr<BluetoothSocket> client_socket = last_socket_;
  last_socket_ = NULL;
  success_callback_count_ = 0;
  error_callback_count_ = 0;

  // Close our end of the client socket.
  client_socket->Disconnect(
      base::Bind(&BluetoothSocketChromeOSTest::SuccessCallback,
                 base::Unretained(this)));

  message_loop_.Run();

  EXPECT_EQ(1U, success_callback_count_);
  client_socket = NULL;
  success_callback_count_ = 0;
  error_callback_count_ = 0;

  // Run a second connection test, this time calling Accept() before the
  // incoming connection comes in.
  server_socket->Accept(
      base::Bind(&BluetoothSocketChromeOSTest::AcceptSuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothSocketChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  fake_bluetooth_device_client->ConnectProfile(
      static_cast<BluetoothDeviceChromeOS*>(device)->object_path(),
      FakeBluetoothProfileManagerClient::kRfcommUuid,
      base::Bind(&base::DoNothing),
      base::Bind(&DoNothingDBusErrorCallback));

  message_loop_.Run();

  EXPECT_EQ(1U, success_callback_count_);
  EXPECT_EQ(0U, error_callback_count_);
  EXPECT_TRUE(last_socket_.get() != NULL);

  // Take ownership of the client socket for the remainder of the test.
  client_socket = last_socket_;
  last_socket_ = NULL;
  success_callback_count_ = 0;
  error_callback_count_ = 0;

  // Close our end of the client socket.
  client_socket->Disconnect(
      base::Bind(&BluetoothSocketChromeOSTest::SuccessCallback,
                 base::Unretained(this)));

  message_loop_.Run();

  EXPECT_EQ(1U, success_callback_count_);
  client_socket = NULL;
  success_callback_count_ = 0;
  error_callback_count_ = 0;

  // Now close the server socket.
  server_socket->Disconnect(
      base::Bind(&BluetoothSocketChromeOSTest::ImmediateSuccessCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1U, success_callback_count_);
}

TEST_F(BluetoothSocketChromeOSTest, ListenBeforeAdapterStart) {
  // Start off with an invisible adapter, register the profile, then make
  // the adapter visible.
  FakeBluetoothAdapterClient* fake_bluetooth_adapter_client =
      static_cast<FakeBluetoothAdapterClient*>(
          DBusThreadManager::Get()->GetBluetoothAdapterClient());
  fake_bluetooth_adapter_client->SetVisible(false);

  adapter_->CreateRfcommService(
      BluetoothUUID(FakeBluetoothProfileManagerClient::kRfcommUuid),
      BluetoothAdapter::kChannelAuto,
      base::Bind(&BluetoothSocketChromeOSTest::CreateServiceSuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothSocketChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1U, success_callback_count_);
  EXPECT_EQ(0U, error_callback_count_);
  EXPECT_TRUE(last_socket_.get() != NULL);

  // Take ownership of the socket for the remainder of the test.
  scoped_refptr<BluetoothSocket> socket = last_socket_;
  last_socket_ = NULL;
  success_callback_count_ = 0;
  error_callback_count_ = 0;

  // But there shouldn't be a profile registered yet.
  FakeBluetoothProfileManagerClient* fake_bluetooth_profile_manager_client =
      static_cast<FakeBluetoothProfileManagerClient*>(
          DBusThreadManager::Get()->GetBluetoothProfileManagerClient());
  FakeBluetoothProfileServiceProvider* profile_service_provider =
      fake_bluetooth_profile_manager_client->GetProfileServiceProvider(
          FakeBluetoothProfileManagerClient::kRfcommUuid);
  EXPECT_TRUE(profile_service_provider == NULL);

  // Make the adapter visible. This should register a profile.
  fake_bluetooth_adapter_client->SetVisible(true);

  profile_service_provider =
      fake_bluetooth_profile_manager_client->GetProfileServiceProvider(
          FakeBluetoothProfileManagerClient::kRfcommUuid);
  EXPECT_TRUE(profile_service_provider != NULL);

  // Cleanup the socket.
  socket->Disconnect(
      base::Bind(&BluetoothSocketChromeOSTest::ImmediateSuccessCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1U, success_callback_count_);
}

TEST_F(BluetoothSocketChromeOSTest, ListenAcrossAdapterRestart) {
  // The fake adapter starts off visible by default.
  FakeBluetoothAdapterClient* fake_bluetooth_adapter_client =
      static_cast<FakeBluetoothAdapterClient*>(
          DBusThreadManager::Get()->GetBluetoothAdapterClient());

  adapter_->CreateRfcommService(
      BluetoothUUID(FakeBluetoothProfileManagerClient::kRfcommUuid),
      BluetoothAdapter::kChannelAuto,
      base::Bind(&BluetoothSocketChromeOSTest::CreateServiceSuccessCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothSocketChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1U, success_callback_count_);
  EXPECT_EQ(0U, error_callback_count_);
  EXPECT_TRUE(last_socket_.get() != NULL);

  // Take ownership of the socket for the remainder of the test.
  scoped_refptr<BluetoothSocket> socket = last_socket_;
  last_socket_ = NULL;
  success_callback_count_ = 0;
  error_callback_count_ = 0;

  // Make sure the profile was registered with the daemon.
  FakeBluetoothProfileManagerClient* fake_bluetooth_profile_manager_client =
      static_cast<FakeBluetoothProfileManagerClient*>(
          DBusThreadManager::Get()->GetBluetoothProfileManagerClient());
  FakeBluetoothProfileServiceProvider* profile_service_provider =
      fake_bluetooth_profile_manager_client->GetProfileServiceProvider(
          FakeBluetoothProfileManagerClient::kRfcommUuid);
  EXPECT_TRUE(profile_service_provider != NULL);

  // Make the adapter invisible, and fiddle with the profile fake to unregister
  // the profile since this doesn't happen automatically.
  fake_bluetooth_adapter_client->SetVisible(false);
  fake_bluetooth_profile_manager_client->UnregisterProfile(
      static_cast<BluetoothSocketChromeOS*>(socket.get())->object_path(),
      base::Bind(&base::DoNothing),
      base::Bind(&DoNothingDBusErrorCallback));

  // Then make the adapter visible again. This should re-register the profile.
  fake_bluetooth_adapter_client->SetVisible(true);

  profile_service_provider =
      fake_bluetooth_profile_manager_client->GetProfileServiceProvider(
          FakeBluetoothProfileManagerClient::kRfcommUuid);
  EXPECT_TRUE(profile_service_provider != NULL);

  // Cleanup the socket.
  socket->Disconnect(
      base::Bind(&BluetoothSocketChromeOSTest::ImmediateSuccessCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1U, success_callback_count_);
}

}  // namespace chromeos
