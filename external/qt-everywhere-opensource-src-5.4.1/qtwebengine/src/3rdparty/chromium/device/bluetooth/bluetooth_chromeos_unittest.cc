// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/dbus/fake_bluetooth_adapter_client.h"
#include "chromeos/dbus/fake_bluetooth_agent_manager_client.h"
#include "chromeos/dbus/fake_bluetooth_device_client.h"
#include "chromeos/dbus/fake_bluetooth_gatt_service_client.h"
#include "chromeos/dbus/fake_bluetooth_input_client.h"
#include "chromeos/dbus/fake_dbus_thread_manager.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_chromeos.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_device_chromeos.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_pairing_chromeos.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using device::BluetoothAdapter;
using device::BluetoothAdapterFactory;
using device::BluetoothDevice;
using device::BluetoothDiscoverySession;
using device::BluetoothUUID;

namespace chromeos {

namespace {

class TestObserver : public BluetoothAdapter::Observer {
 public:
  TestObserver(scoped_refptr<BluetoothAdapter> adapter)
      : present_changed_count_(0),
        powered_changed_count_(0),
        discoverable_changed_count_(0),
        discovering_changed_count_(0),
        last_present_(false),
        last_powered_(false),
        last_discovering_(false),
        device_added_count_(0),
        device_changed_count_(0),
        device_removed_count_(0),
        last_device_(NULL),
        adapter_(adapter) {
    adapter_->AddObserver(this);
  }

  virtual ~TestObserver() {
    adapter_->RemoveObserver(this);
  }

  virtual void AdapterPresentChanged(BluetoothAdapter* adapter,
                                     bool present) OVERRIDE {
    EXPECT_EQ(adapter_, adapter);

    ++present_changed_count_;
    last_present_ = present;
  }

  virtual void AdapterPoweredChanged(BluetoothAdapter* adapter,
                                     bool powered) OVERRIDE {
    EXPECT_EQ(adapter_, adapter);

    ++powered_changed_count_;
    last_powered_ = powered;
  }

  virtual void AdapterDiscoverableChanged(BluetoothAdapter* adapter,
                                          bool discoverable) OVERRIDE {
    EXPECT_EQ(adapter_, adapter);

    ++discoverable_changed_count_;
  }

  virtual void AdapterDiscoveringChanged(BluetoothAdapter* adapter,
                                         bool discovering) OVERRIDE {
    EXPECT_EQ(adapter_, adapter);

    ++discovering_changed_count_;
    last_discovering_ = discovering;
  }

  virtual void DeviceAdded(BluetoothAdapter* adapter,
                           BluetoothDevice* device) OVERRIDE {
    EXPECT_EQ(adapter_, adapter);

    ++device_added_count_;
    last_device_ = device;
    last_device_address_ = device->GetAddress();

    QuitMessageLoop();
  }

  virtual void DeviceChanged(BluetoothAdapter* adapter,
                             BluetoothDevice* device) OVERRIDE {
    EXPECT_EQ(adapter_, adapter);

    ++device_changed_count_;
    last_device_ = device;
    last_device_address_ = device->GetAddress();

    QuitMessageLoop();
  }

  virtual void DeviceRemoved(BluetoothAdapter* adapter,
                             BluetoothDevice* device) OVERRIDE {
    EXPECT_EQ(adapter_, adapter);

    ++device_removed_count_;
    // Can't save device, it may be freed
    last_device_address_ = device->GetAddress();

    QuitMessageLoop();
  }

  int present_changed_count_;
  int powered_changed_count_;
  int discoverable_changed_count_;
  int discovering_changed_count_;
  bool last_present_;
  bool last_powered_;
  bool last_discovering_;
  int device_added_count_;
  int device_changed_count_;
  int device_removed_count_;
  BluetoothDevice* last_device_;
  std::string last_device_address_;

 private:
  // Some tests use a message loop since background processing is simulated;
  // break out of those loops.
  void QuitMessageLoop() {
    if (base::MessageLoop::current() &&
        base::MessageLoop::current()->is_running())
      base::MessageLoop::current()->Quit();
  }

  scoped_refptr<BluetoothAdapter> adapter_;
};

}  // namespace

class TestPairingDelegate : public BluetoothDevice::PairingDelegate {
 public:
  TestPairingDelegate()
      : call_count_(0),
        request_pincode_count_(0),
        request_passkey_count_(0),
        display_pincode_count_(0),
        display_passkey_count_(0),
        keys_entered_count_(0),
        confirm_passkey_count_(0),
        authorize_pairing_count_(0),
        last_passkey_(9999999U),
        last_entered_(999U) {}
  virtual ~TestPairingDelegate() {}

  virtual void RequestPinCode(BluetoothDevice* device) OVERRIDE {
    ++call_count_;
    ++request_pincode_count_;
    QuitMessageLoop();
  }

  virtual void RequestPasskey(BluetoothDevice* device) OVERRIDE {
    ++call_count_;
    ++request_passkey_count_;
    QuitMessageLoop();
  }

  virtual void DisplayPinCode(BluetoothDevice* device,
                              const std::string& pincode) OVERRIDE {
    ++call_count_;
    ++display_pincode_count_;
    last_pincode_ = pincode;
    QuitMessageLoop();
  }

  virtual void DisplayPasskey(BluetoothDevice* device,
                              uint32 passkey) OVERRIDE {
    ++call_count_;
    ++display_passkey_count_;
    last_passkey_ = passkey;
    QuitMessageLoop();
  }

  virtual void KeysEntered(BluetoothDevice* device, uint32 entered) OVERRIDE {
    ++call_count_;
    ++keys_entered_count_;
    last_entered_ = entered;
    QuitMessageLoop();
  }

  virtual void ConfirmPasskey(BluetoothDevice* device,
                              uint32 passkey) OVERRIDE {
    ++call_count_;
    ++confirm_passkey_count_;
    last_passkey_ = passkey;
    QuitMessageLoop();
  }

  virtual void AuthorizePairing(BluetoothDevice* device) OVERRIDE {
    ++call_count_;
    ++authorize_pairing_count_;
    QuitMessageLoop();
  }

  int call_count_;
  int request_pincode_count_;
  int request_passkey_count_;
  int display_pincode_count_;
  int display_passkey_count_;
  int keys_entered_count_;
  int confirm_passkey_count_;
  int authorize_pairing_count_;
  uint32 last_passkey_;
  uint32 last_entered_;
  std::string last_pincode_;

  private:
   // Some tests use a message loop since background processing is simulated;
   // break out of those loops.
   void QuitMessageLoop() {
     if (base::MessageLoop::current() &&
         base::MessageLoop::current()->is_running())
       base::MessageLoop::current()->Quit();
   }
};

class BluetoothChromeOSTest : public testing::Test {
 public:
  virtual void SetUp() {
    FakeDBusThreadManager* fake_dbus_thread_manager = new FakeDBusThreadManager;
    fake_bluetooth_adapter_client_ = new FakeBluetoothAdapterClient;
    fake_dbus_thread_manager->SetBluetoothAdapterClient(
        scoped_ptr<BluetoothAdapterClient>(fake_bluetooth_adapter_client_));
    fake_bluetooth_device_client_ = new FakeBluetoothDeviceClient;
    fake_dbus_thread_manager->SetBluetoothDeviceClient(
        scoped_ptr<BluetoothDeviceClient>(fake_bluetooth_device_client_));
    fake_dbus_thread_manager->SetBluetoothInputClient(
        scoped_ptr<BluetoothInputClient>(new FakeBluetoothInputClient));
    fake_dbus_thread_manager->SetBluetoothAgentManagerClient(
        scoped_ptr<BluetoothAgentManagerClient>(
            new FakeBluetoothAgentManagerClient));
    fake_dbus_thread_manager->SetBluetoothGattServiceClient(
        scoped_ptr<BluetoothGattServiceClient>(
            new FakeBluetoothGattServiceClient));
    DBusThreadManager::InitializeForTesting(fake_dbus_thread_manager);

    fake_bluetooth_adapter_client_->SetSimulationIntervalMs(10);

    callback_count_ = 0;
    error_callback_count_ = 0;
    last_connect_error_ = BluetoothDevice::ERROR_UNKNOWN;
    last_client_error_ = "";
  }

  virtual void TearDown() {
    for (ScopedVector<BluetoothDiscoverySession>::iterator iter =
            discovery_sessions_.begin();
         iter != discovery_sessions_.end();
         ++iter) {
      BluetoothDiscoverySession* session = *iter;
      if (!session->IsActive())
        continue;
      callback_count_ = 0;
      session->Stop(
          base::Bind(&BluetoothChromeOSTest::Callback,
                     base::Unretained(this)),
          base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                     base::Unretained(this)));
      message_loop_.Run();
      ASSERT_EQ(1, callback_count_);
    }
    discovery_sessions_.clear();
    adapter_ = NULL;
    DBusThreadManager::Shutdown();
  }

  // Generic callbacks
  void Callback() {
    ++callback_count_;
    QuitMessageLoop();
  }

  void DiscoverySessionCallback(
      scoped_ptr<BluetoothDiscoverySession> discovery_session) {
    ++callback_count_;
    discovery_sessions_.push_back(discovery_session.release());
    QuitMessageLoop();
  }

  void ErrorCallback() {
    ++error_callback_count_;
    QuitMessageLoop();
  }

  void DBusErrorCallback(const std::string& error_name,
                         const std::string& error_message) {
    ++error_callback_count_;
    last_client_error_ = error_name;
    QuitMessageLoop();
  }

  void ConnectErrorCallback(BluetoothDevice::ConnectErrorCode error) {
    ++error_callback_count_;
    last_connect_error_ = error;
  }

  // Call to fill the adapter_ member with a BluetoothAdapter instance.
  void GetAdapter() {
    adapter_ = new BluetoothAdapterChromeOS();
    ASSERT_TRUE(adapter_.get() != NULL);
    ASSERT_TRUE(adapter_->IsInitialized());
  }

  // Run a discovery phase until the named device is detected, or if the named
  // device is not created, the discovery process ends without finding it.
  //
  // The correct behavior of discovery is tested by the "Discovery" test case
  // without using this function.
  void DiscoverDevice(const std::string& address) {
    ASSERT_TRUE(adapter_.get() != NULL);
    ASSERT_TRUE(base::MessageLoop::current() != NULL);
    fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

    TestObserver observer(adapter_);

    adapter_->SetPowered(
        true,
        base::Bind(&BluetoothChromeOSTest::Callback,
                   base::Unretained(this)),
        base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                   base::Unretained(this)));
    adapter_->StartDiscoverySession(
        base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                   base::Unretained(this)),
        base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                   base::Unretained(this)));
    base::MessageLoop::current()->Run();
    ASSERT_EQ(2, callback_count_);
    ASSERT_EQ(0, error_callback_count_);
    ASSERT_EQ((size_t)1, discovery_sessions_.size());
    ASSERT_TRUE(discovery_sessions_[0]->IsActive());
    callback_count_ = 0;

    ASSERT_TRUE(adapter_->IsPowered());
    ASSERT_TRUE(adapter_->IsDiscovering());

    while (!observer.device_removed_count_ &&
           observer.last_device_address_ != address)
      base::MessageLoop::current()->Run();

    discovery_sessions_[0]->Stop(
        base::Bind(&BluetoothChromeOSTest::Callback,
                   base::Unretained(this)),
        base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                   base::Unretained(this)));
    base::MessageLoop::current()->Run();
    ASSERT_EQ(1, callback_count_);
    ASSERT_EQ(0, error_callback_count_);
    callback_count_ = 0;

    ASSERT_FALSE(adapter_->IsDiscovering());
  }

  // Run a discovery phase so we have devices that can be paired with.
  void DiscoverDevices() {
    // Pass an invalid address for the device so that the discovery process
    // completes with all devices.
    DiscoverDevice("does not exist");
  }

 protected:
  base::MessageLoop message_loop_;
  FakeBluetoothAdapterClient* fake_bluetooth_adapter_client_;
  FakeBluetoothDeviceClient* fake_bluetooth_device_client_;
  scoped_refptr<BluetoothAdapter> adapter_;

  int callback_count_;
  int error_callback_count_;
  enum BluetoothDevice::ConnectErrorCode last_connect_error_;
  std::string last_client_error_;
  ScopedVector<BluetoothDiscoverySession> discovery_sessions_;

 private:
  // Some tests use a message loop since background processing is simulated;
  // break out of those loops.
  void QuitMessageLoop() {
    if (base::MessageLoop::current() &&
        base::MessageLoop::current()->is_running())
      base::MessageLoop::current()->Quit();
  }
};

TEST_F(BluetoothChromeOSTest, AlreadyPresent) {
  GetAdapter();

  // This verifies that the class gets the list of adapters when created;
  // and initializes with an existing adapter if there is one.
  EXPECT_TRUE(adapter_->IsPresent());
  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_EQ(FakeBluetoothAdapterClient::kAdapterAddress,
            adapter_->GetAddress());
  EXPECT_FALSE(adapter_->IsDiscovering());

  // There should be a device
  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  EXPECT_EQ(1U, devices.size());
  EXPECT_EQ(FakeBluetoothDeviceClient::kPairedDeviceAddress,
            devices[0]->GetAddress());
}

TEST_F(BluetoothChromeOSTest, BecomePresent) {
  fake_bluetooth_adapter_client_->SetVisible(false);
  GetAdapter();
  ASSERT_FALSE(adapter_->IsPresent());

  // Install an observer; expect the AdapterPresentChanged to be called
  // with true, and IsPresent() to return true.
  TestObserver observer(adapter_);

  fake_bluetooth_adapter_client_->SetVisible(true);

  EXPECT_EQ(1, observer.present_changed_count_);
  EXPECT_TRUE(observer.last_present_);

  EXPECT_TRUE(adapter_->IsPresent());

  // We should have had a device announced.
  EXPECT_EQ(1, observer.device_added_count_);
  EXPECT_EQ(FakeBluetoothDeviceClient::kPairedDeviceAddress,
            observer.last_device_address_);

  // Other callbacks shouldn't be called if the values are false.
  EXPECT_EQ(0, observer.powered_changed_count_);
  EXPECT_EQ(0, observer.discovering_changed_count_);
  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_FALSE(adapter_->IsDiscovering());
}

TEST_F(BluetoothChromeOSTest, BecomeNotPresent) {
  GetAdapter();
  ASSERT_TRUE(adapter_->IsPresent());

  // Install an observer; expect the AdapterPresentChanged to be called
  // with false, and IsPresent() to return false.
  TestObserver observer(adapter_);

  fake_bluetooth_adapter_client_->SetVisible(false);

  EXPECT_EQ(1, observer.present_changed_count_);
  EXPECT_FALSE(observer.last_present_);

  EXPECT_FALSE(adapter_->IsPresent());

  // We should have had a device removed.
  EXPECT_EQ(1, observer.device_removed_count_);
  EXPECT_EQ(FakeBluetoothDeviceClient::kPairedDeviceAddress,
            observer.last_device_address_);

  // Other callbacks shouldn't be called since the values are false.
  EXPECT_EQ(0, observer.powered_changed_count_);
  EXPECT_EQ(0, observer.discovering_changed_count_);
  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_FALSE(adapter_->IsDiscovering());
}

TEST_F(BluetoothChromeOSTest, SecondAdapter) {
  GetAdapter();
  ASSERT_TRUE(adapter_->IsPresent());

  // Install an observer, then add a second adapter. Nothing should change,
  // we ignore the second adapter.
  TestObserver observer(adapter_);

  fake_bluetooth_adapter_client_->SetSecondVisible(true);

  EXPECT_EQ(0, observer.present_changed_count_);

  EXPECT_TRUE(adapter_->IsPresent());
  EXPECT_EQ(FakeBluetoothAdapterClient::kAdapterAddress,
            adapter_->GetAddress());

  // Try removing the first adapter, we should now act as if the adapter
  // is no longer present rather than fall back to the second.
  fake_bluetooth_adapter_client_->SetVisible(false);

  EXPECT_EQ(1, observer.present_changed_count_);
  EXPECT_FALSE(observer.last_present_);

  EXPECT_FALSE(adapter_->IsPresent());

  // We should have had a device removed.
  EXPECT_EQ(1, observer.device_removed_count_);
  EXPECT_EQ(FakeBluetoothDeviceClient::kPairedDeviceAddress,
            observer.last_device_address_);

  // Other callbacks shouldn't be called since the values are false.
  EXPECT_EQ(0, observer.powered_changed_count_);
  EXPECT_EQ(0, observer.discovering_changed_count_);
  EXPECT_FALSE(adapter_->IsPowered());
  EXPECT_FALSE(adapter_->IsDiscovering());

  observer.device_removed_count_ = 0;

  // Removing the second adapter shouldn't set anything either.
  fake_bluetooth_adapter_client_->SetSecondVisible(false);

  EXPECT_EQ(0, observer.device_removed_count_);
  EXPECT_EQ(0, observer.powered_changed_count_);
  EXPECT_EQ(0, observer.discovering_changed_count_);
}

TEST_F(BluetoothChromeOSTest, BecomePowered) {
  GetAdapter();
  ASSERT_FALSE(adapter_->IsPowered());

  // Install an observer; expect the AdapterPoweredChanged to be called
  // with true, and IsPowered() to return true.
  TestObserver observer(adapter_);

  adapter_->SetPowered(
      true,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.powered_changed_count_);
  EXPECT_TRUE(observer.last_powered_);

  EXPECT_TRUE(adapter_->IsPowered());
}

TEST_F(BluetoothChromeOSTest, BecomeNotPowered) {
  GetAdapter();
  adapter_->SetPowered(
      true,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  ASSERT_TRUE(adapter_->IsPowered());

  // Install an observer; expect the AdapterPoweredChanged to be called
  // with false, and IsPowered() to return false.
  TestObserver observer(adapter_);

  adapter_->SetPowered(
      false,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.powered_changed_count_);
  EXPECT_FALSE(observer.last_powered_);

  EXPECT_FALSE(adapter_->IsPowered());
}

TEST_F(BluetoothChromeOSTest, ChangeAdapterName) {
  GetAdapter();

  static const std::string new_name(".__.");

  adapter_->SetName(
      new_name,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(new_name, adapter_->GetName());
}

TEST_F(BluetoothChromeOSTest, BecomeDiscoverable) {
  GetAdapter();
  ASSERT_FALSE(adapter_->IsDiscoverable());

  // Install an observer; expect the AdapterDiscoverableChanged to be called
  // with true, and IsDiscoverable() to return true.
  TestObserver observer(adapter_);

  adapter_->SetDiscoverable(
      true,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.discoverable_changed_count_);

  EXPECT_TRUE(adapter_->IsDiscoverable());
}

TEST_F(BluetoothChromeOSTest, BecomeNotDiscoverable) {
  GetAdapter();
  adapter_->SetDiscoverable(
      true,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  ASSERT_TRUE(adapter_->IsDiscoverable());

  // Install an observer; expect the AdapterDiscoverableChanged to be called
  // with false, and IsDiscoverable() to return false.
  TestObserver observer(adapter_);

  adapter_->SetDiscoverable(
      false,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.discoverable_changed_count_);

  EXPECT_FALSE(adapter_->IsDiscoverable());
}

TEST_F(BluetoothChromeOSTest, StopDiscovery) {
  GetAdapter();

  adapter_->SetPowered(
      true,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  adapter_->StartDiscoverySession(
      base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  message_loop_.Run();
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)1, discovery_sessions_.size());
  ASSERT_TRUE(discovery_sessions_[0]->IsActive());

  // Install an observer; aside from the callback, expect the
  // AdapterDiscoveringChanged method to be called and no longer to be
  // discovering,
  TestObserver observer(adapter_);

  discovery_sessions_[0]->Stop(
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  message_loop_.Run();
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.discovering_changed_count_);
  EXPECT_FALSE(observer.last_discovering_);

  EXPECT_FALSE(adapter_->IsDiscovering());
}

TEST_F(BluetoothChromeOSTest, Discovery) {
  // Test a simulated discovery session.
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);
  GetAdapter();

  TestObserver observer(adapter_);

  adapter_->SetPowered(
      true,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  adapter_->StartDiscoverySession(
      base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  message_loop_.Run();
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)1, discovery_sessions_.size());
  ASSERT_TRUE(discovery_sessions_[0]->IsActive());

  // First two devices to appear.
  message_loop_.Run();

  EXPECT_EQ(2, observer.device_added_count_);
  EXPECT_EQ(FakeBluetoothDeviceClient::kLowEnergyAddress,
            observer.last_device_address_);

  // Next we should get another two devices...
  message_loop_.Run();
  EXPECT_EQ(4, observer.device_added_count_);

  // Okay, let's run forward until a device is actually removed...
  while (!observer.device_removed_count_)
    message_loop_.Run();

  EXPECT_EQ(1, observer.device_removed_count_);
  EXPECT_EQ(FakeBluetoothDeviceClient::kVanishingDeviceAddress,
            observer.last_device_address_);
}

TEST_F(BluetoothChromeOSTest, PoweredAndDiscovering) {
  GetAdapter();
  adapter_->SetPowered(
      true,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  adapter_->StartDiscoverySession(
      base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  message_loop_.Run();
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  callback_count_ = 0;
  ASSERT_EQ((size_t)1, discovery_sessions_.size());
  ASSERT_TRUE(discovery_sessions_[0]->IsActive());

  // Stop the timers that the simulation uses
  fake_bluetooth_device_client_->EndDiscoverySimulation(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath));

  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_TRUE(adapter_->IsDiscovering());

  fake_bluetooth_adapter_client_->SetVisible(false);
  ASSERT_FALSE(adapter_->IsPresent());
  ASSERT_FALSE(discovery_sessions_[0]->IsActive());

  // Install an observer; expect the AdapterPresentChanged,
  // AdapterPoweredChanged and AdapterDiscoveringChanged methods to be called
  // with true, and IsPresent(), IsPowered() and IsDiscovering() to all
  // return true.
  TestObserver observer(adapter_);

  fake_bluetooth_adapter_client_->SetVisible(true);

  EXPECT_EQ(1, observer.present_changed_count_);
  EXPECT_TRUE(observer.last_present_);
  EXPECT_TRUE(adapter_->IsPresent());

  EXPECT_EQ(1, observer.powered_changed_count_);
  EXPECT_TRUE(observer.last_powered_);
  EXPECT_TRUE(adapter_->IsPowered());

  EXPECT_EQ(1, observer.discovering_changed_count_);
  EXPECT_TRUE(observer.last_discovering_);
  EXPECT_TRUE(adapter_->IsDiscovering());

  observer.present_changed_count_ = 0;
  observer.powered_changed_count_ = 0;
  observer.discovering_changed_count_ = 0;

  // Now mark the adapter not present again. Expect the methods to be called
  // again, to reset the properties back to false
  fake_bluetooth_adapter_client_->SetVisible(false);

  EXPECT_EQ(1, observer.present_changed_count_);
  EXPECT_FALSE(observer.last_present_);
  EXPECT_FALSE(adapter_->IsPresent());

  EXPECT_EQ(1, observer.powered_changed_count_);
  EXPECT_FALSE(observer.last_powered_);
  EXPECT_FALSE(adapter_->IsPowered());

  EXPECT_EQ(1, observer.discovering_changed_count_);
  EXPECT_FALSE(observer.last_discovering_);
  EXPECT_FALSE(adapter_->IsDiscovering());
}

// This unit test asserts that the basic reference counting logic works
// correctly for discovery requests done via the BluetoothAdapter.
TEST_F(BluetoothChromeOSTest, MultipleDiscoverySessions) {
  GetAdapter();
  adapter_->SetPowered(
      true,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(adapter_->IsPowered());
  callback_count_ = 0;

  TestObserver observer(adapter_);

  EXPECT_EQ(0, observer.discovering_changed_count_);
  EXPECT_FALSE(observer.last_discovering_);
  EXPECT_FALSE(adapter_->IsDiscovering());

  // Request device discovery 3 times.
  for (int i = 0; i < 3; i++) {
    adapter_->StartDiscoverySession(
      base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  }
  // Run only once, as there should have been one D-Bus call.
  message_loop_.Run();

  // The observer should have received the discovering changed event exactly
  // once, the success callback should have been called 3 times and the adapter
  // should be discovering.
  EXPECT_EQ(1, observer.discovering_changed_count_);
  EXPECT_EQ(3, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering_);
  EXPECT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)3, discovery_sessions_.size());

  // Request to stop discovery twice.
  for (int i = 0; i < 2; i++) {
    discovery_sessions_[i]->Stop(
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  }

  // The observer should have received no additional discovering changed events,
  // the success callback should have been called 2 times and the adapter should
  // still be discovering.
  EXPECT_EQ(1, observer.discovering_changed_count_);
  EXPECT_EQ(5, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering_);
  EXPECT_TRUE(adapter_->IsDiscovering());
  EXPECT_TRUE(adapter_->IsDiscovering());
  EXPECT_FALSE(discovery_sessions_[0]->IsActive());
  EXPECT_FALSE(discovery_sessions_[1]->IsActive());
  EXPECT_TRUE(discovery_sessions_[2]->IsActive());

  // Request device discovery 3 times.
  for (int i = 0; i < 3; i++) {
    adapter_->StartDiscoverySession(
      base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  }

  // The observer should have received no additional discovering changed events,
  // the success callback should have been called 3 times and the adapter should
  // still be discovering.
  EXPECT_EQ(1, observer.discovering_changed_count_);
  EXPECT_EQ(8, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering_);
  EXPECT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)6, discovery_sessions_.size());

  // Request to stop discovery 4 times.
  for (int i = 2; i < 6; i++) {
    discovery_sessions_[i]->Stop(
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  }
  // Run only once, as there should have been one D-Bus call.
  message_loop_.Run();

  // The observer should have received the discovering changed event exactly
  // once, the success callback should have been called 4 times and the adapter
  // should no longer be discovering.
  EXPECT_EQ(2, observer.discovering_changed_count_);
  EXPECT_EQ(12, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_FALSE(observer.last_discovering_);
  EXPECT_FALSE(adapter_->IsDiscovering());

  // All discovery sessions should be inactive.
  for (int i = 0; i < 6; i++)
    EXPECT_FALSE(discovery_sessions_[i]->IsActive());

  // Request to stop discovery on of the inactive sessions.
  discovery_sessions_[0]->Stop(
    base::Bind(&BluetoothChromeOSTest::Callback,
               base::Unretained(this)),
    base::Bind(&BluetoothChromeOSTest::ErrorCallback,
               base::Unretained(this)));

  // The call should have failed.
  EXPECT_EQ(2, observer.discovering_changed_count_);
  EXPECT_EQ(12, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_FALSE(observer.last_discovering_);
  EXPECT_FALSE(adapter_->IsDiscovering());
}

// This unit test asserts that the reference counting logic works correctly in
// the cases when the adapter gets reset and D-Bus calls are made outside of
// the BluetoothAdapter.
TEST_F(BluetoothChromeOSTest,
       UnexpectedChangesDuringMultipleDiscoverySessions) {
  GetAdapter();
  adapter_->SetPowered(
      true,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(adapter_->IsPowered());
  callback_count_ = 0;

  TestObserver observer(adapter_);

  EXPECT_EQ(0, observer.discovering_changed_count_);
  EXPECT_FALSE(observer.last_discovering_);
  EXPECT_FALSE(adapter_->IsDiscovering());

  // Request device discovery 3 times.
  for (int i = 0; i < 3; i++) {
    adapter_->StartDiscoverySession(
      base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  }
  // Run only once, as there should have been one D-Bus call.
  message_loop_.Run();

  // The observer should have received the discovering changed event exactly
  // once, the success callback should have been called 3 times and the adapter
  // should be discovering.
  EXPECT_EQ(1, observer.discovering_changed_count_);
  EXPECT_EQ(3, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering_);
  EXPECT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)3, discovery_sessions_.size());

  for (int i = 0; i < 3; i++)
    EXPECT_TRUE(discovery_sessions_[i]->IsActive());

  // Stop the timers that the simulation uses
  fake_bluetooth_device_client_->EndDiscoverySimulation(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath));

  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_TRUE(adapter_->IsDiscovering());

  // Stop device discovery behind the adapter. The adapter and the observer
  // should be notified of the change and the reference count should be reset.
  // Even though FakeBluetoothAdapterClient does its own reference counting and
  // we called 3 BluetoothAdapter::StartDiscoverySession 3 times, the
  // FakeBluetoothAdapterClient's count should be only 1 and a single call to
  // FakeBluetoothAdapterClient::StopDiscovery should work.
  fake_bluetooth_adapter_client_->StopDiscovery(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::DBusErrorCallback,
                 base::Unretained(this)));
  message_loop_.Run();
  EXPECT_EQ(2, observer.discovering_changed_count_);
  EXPECT_EQ(4, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_FALSE(observer.last_discovering_);
  EXPECT_FALSE(adapter_->IsDiscovering());

  // All discovery session instances should have been updated.
  for (int i = 0; i < 3; i++)
    EXPECT_FALSE(discovery_sessions_[i]->IsActive());
  discovery_sessions_.clear();

  // It should be possible to successfully start discovery.
  for (int i = 0; i < 2; i++) {
    adapter_->StartDiscoverySession(
      base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  }
  // Run only once, as there should have been one D-Bus call.
  message_loop_.Run();
  EXPECT_EQ(3, observer.discovering_changed_count_);
  EXPECT_EQ(6, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering_);
  EXPECT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)2, discovery_sessions_.size());

  for (int i = 0; i < 2; i++)
    EXPECT_TRUE(discovery_sessions_[i]->IsActive());

  fake_bluetooth_device_client_->EndDiscoverySimulation(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath));

  // Make the adapter disappear and appear. This will make it come back as
  // discovering. When this happens, the reference count should become and
  // remain 0 as no new request was made through the BluetoothAdapter.
  fake_bluetooth_adapter_client_->SetVisible(false);
  ASSERT_FALSE(adapter_->IsPresent());
  EXPECT_EQ(4, observer.discovering_changed_count_);
  EXPECT_EQ(6, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_FALSE(observer.last_discovering_);
  EXPECT_FALSE(adapter_->IsDiscovering());

  for (int i = 0; i < 2; i++)
    EXPECT_FALSE(discovery_sessions_[i]->IsActive());
  discovery_sessions_.clear();

  fake_bluetooth_adapter_client_->SetVisible(true);
  ASSERT_TRUE(adapter_->IsPresent());
  EXPECT_EQ(5, observer.discovering_changed_count_);
  EXPECT_EQ(6, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering_);
  EXPECT_TRUE(adapter_->IsDiscovering());

  // Start and stop discovery. At this point, FakeBluetoothAdapterClient has
  // a reference count that is equal to 1. Pretend that this was done by an
  // application other than us. Starting and stopping discovery will succeed
  // but it won't cause the discovery state to change.
  adapter_->StartDiscoverySession(
    base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
               base::Unretained(this)),
    base::Bind(&BluetoothChromeOSTest::ErrorCallback,
               base::Unretained(this)));
  message_loop_.Run();  // Run the loop, as there should have been a D-Bus call.
  EXPECT_EQ(5, observer.discovering_changed_count_);
  EXPECT_EQ(7, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering_);
  EXPECT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)1, discovery_sessions_.size());
  EXPECT_TRUE(discovery_sessions_[0]->IsActive());

  discovery_sessions_[0]->Stop(
    base::Bind(&BluetoothChromeOSTest::Callback,
               base::Unretained(this)),
    base::Bind(&BluetoothChromeOSTest::ErrorCallback,
               base::Unretained(this)));
  message_loop_.Run();  // Run the loop, as there should have been a D-Bus call.
  EXPECT_EQ(5, observer.discovering_changed_count_);
  EXPECT_EQ(8, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering_);
  EXPECT_TRUE(adapter_->IsDiscovering());
  EXPECT_FALSE(discovery_sessions_[0]->IsActive());
  discovery_sessions_.clear();

  // Start discovery again.
  adapter_->StartDiscoverySession(
    base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
               base::Unretained(this)),
    base::Bind(&BluetoothChromeOSTest::ErrorCallback,
               base::Unretained(this)));
  message_loop_.Run();  // Run the loop, as there should have been a D-Bus call.
  EXPECT_EQ(5, observer.discovering_changed_count_);
  EXPECT_EQ(9, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering_);
  EXPECT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)1, discovery_sessions_.size());
  EXPECT_TRUE(discovery_sessions_[0]->IsActive());

  // Stop discovery via D-Bus. The fake client's reference count will drop but
  // the discovery state won't change since our BluetoothAdapter also just
  // requested it via D-Bus.
  fake_bluetooth_adapter_client_->StopDiscovery(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::DBusErrorCallback,
                 base::Unretained(this)));
  message_loop_.Run();
  EXPECT_EQ(5, observer.discovering_changed_count_);
  EXPECT_EQ(10, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering_);
  EXPECT_TRUE(adapter_->IsDiscovering());

  // Now end the discovery session. This should change the adapter's discovery
  // state.
  discovery_sessions_[0]->Stop(
    base::Bind(&BluetoothChromeOSTest::Callback,
               base::Unretained(this)),
    base::Bind(&BluetoothChromeOSTest::ErrorCallback,
               base::Unretained(this)));
  message_loop_.Run();
  EXPECT_EQ(6, observer.discovering_changed_count_);
  EXPECT_EQ(11, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_FALSE(observer.last_discovering_);
  EXPECT_FALSE(adapter_->IsDiscovering());
  EXPECT_FALSE(discovery_sessions_[0]->IsActive());
}

TEST_F(BluetoothChromeOSTest, InvalidatedDiscoverySessions) {
  GetAdapter();
  adapter_->SetPowered(
      true,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(adapter_->IsPowered());
  callback_count_ = 0;

  TestObserver observer(adapter_);

  EXPECT_EQ(0, observer.discovering_changed_count_);
  EXPECT_FALSE(observer.last_discovering_);
  EXPECT_FALSE(adapter_->IsDiscovering());

  // Request device discovery 3 times.
  for (int i = 0; i < 3; i++) {
    adapter_->StartDiscoverySession(
      base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  }
  // Run only once, as there should have been one D-Bus call.
  message_loop_.Run();

  // The observer should have received the discovering changed event exactly
  // once, the success callback should have been called 3 times and the adapter
  // should be discovering.
  EXPECT_EQ(1, observer.discovering_changed_count_);
  EXPECT_EQ(3, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering_);
  EXPECT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)3, discovery_sessions_.size());

  for (int i = 0; i < 3; i++)
    EXPECT_TRUE(discovery_sessions_[i]->IsActive());

  // Stop the timers that the simulation uses
  fake_bluetooth_device_client_->EndDiscoverySimulation(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath));

  ASSERT_TRUE(adapter_->IsPowered());
  ASSERT_TRUE(adapter_->IsDiscovering());

  // Delete all but one discovery session.
  discovery_sessions_.pop_back();
  discovery_sessions_.pop_back();
  ASSERT_EQ((size_t)1, discovery_sessions_.size());
  EXPECT_TRUE(discovery_sessions_[0]->IsActive());
  EXPECT_TRUE(adapter_->IsDiscovering());

  // Stop device discovery behind the adapter. The one active discovery session
  // should become inactive, but more importantly, we shouldn't run into any
  // memory errors as the sessions that we explicitly deleted should get
  // cleaned up.
  fake_bluetooth_adapter_client_->StopDiscovery(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::DBusErrorCallback,
                 base::Unretained(this)));
  message_loop_.Run();
  EXPECT_EQ(2, observer.discovering_changed_count_);
  EXPECT_EQ(4, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_FALSE(observer.last_discovering_);
  EXPECT_FALSE(adapter_->IsDiscovering());
  EXPECT_FALSE(discovery_sessions_[0]->IsActive());
}

TEST_F(BluetoothChromeOSTest, QueuedDiscoveryRequests) {
  GetAdapter();

  adapter_->SetPowered(
      true,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(adapter_->IsPowered());
  callback_count_ = 0;

  TestObserver observer(adapter_);

  EXPECT_EQ(0, observer.discovering_changed_count_);
  EXPECT_FALSE(observer.last_discovering_);
  EXPECT_FALSE(adapter_->IsDiscovering());

  // Request to start discovery. The call should be pending.
  adapter_->StartDiscoverySession(
    base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
               base::Unretained(this)),
    base::Bind(&BluetoothChromeOSTest::ErrorCallback,
               base::Unretained(this)));
  EXPECT_EQ(0, callback_count_);

  fake_bluetooth_device_client_->EndDiscoverySimulation(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath));

  // The underlying adapter has started discovery, but our call hasn't returned
  // yet.
  EXPECT_EQ(1, observer.discovering_changed_count_);
  EXPECT_TRUE(observer.last_discovering_);
  EXPECT_TRUE(adapter_->IsDiscovering());
  EXPECT_TRUE(discovery_sessions_.empty());

  // Request to start discovery twice. These should get queued and there should
  // be no change in state.
  for (int i = 0; i < 2; i++) {
    adapter_->StartDiscoverySession(
      base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  }
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(1, observer.discovering_changed_count_);
  EXPECT_TRUE(observer.last_discovering_);
  EXPECT_TRUE(adapter_->IsDiscovering());
  EXPECT_TRUE(discovery_sessions_.empty());

  // Process the pending call. The queued calls should execute and the discovery
  // session reference count should increase.
  message_loop_.Run();
  EXPECT_EQ(3, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(1, observer.discovering_changed_count_);
  EXPECT_TRUE(observer.last_discovering_);
  EXPECT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)3, discovery_sessions_.size());

  // Verify the reference count by removing sessions 3 times. The last request
  // should remain pending.
  for (int i = 0; i < 3; i++) {
    discovery_sessions_[i]->Stop(
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  }
  EXPECT_EQ(5, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(2, observer.discovering_changed_count_);
  EXPECT_FALSE(observer.last_discovering_);
  EXPECT_FALSE(adapter_->IsDiscovering());
  EXPECT_FALSE(discovery_sessions_[0]->IsActive());
  EXPECT_FALSE(discovery_sessions_[1]->IsActive());
  EXPECT_TRUE(discovery_sessions_[2]->IsActive());

  // Request to stop the session whose call is pending should fail.
  discovery_sessions_[2]->Stop(
    base::Bind(&BluetoothChromeOSTest::Callback,
               base::Unretained(this)),
    base::Bind(&BluetoothChromeOSTest::ErrorCallback,
               base::Unretained(this)));
  EXPECT_EQ(5, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(2, observer.discovering_changed_count_);
  EXPECT_FALSE(observer.last_discovering_);
  EXPECT_FALSE(adapter_->IsDiscovering());
  EXPECT_TRUE(discovery_sessions_[2]->IsActive());

  // Request to start should get queued.
  adapter_->StartDiscoverySession(
    base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
               base::Unretained(this)),
    base::Bind(&BluetoothChromeOSTest::ErrorCallback,
               base::Unretained(this)));
  EXPECT_EQ(5, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(2, observer.discovering_changed_count_);
  EXPECT_FALSE(observer.last_discovering_);
  EXPECT_FALSE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)3, discovery_sessions_.size());

  // Run the pending request.
  message_loop_.Run();
  EXPECT_EQ(6, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(3, observer.discovering_changed_count_);
  EXPECT_TRUE(observer.last_discovering_);
  EXPECT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)3, discovery_sessions_.size());
  EXPECT_FALSE(discovery_sessions_[2]->IsActive());

  // The queued request to start discovery should have been issued but is still
  // pending. Run the loop and verify.
  message_loop_.Run();
  EXPECT_EQ(7, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(3, observer.discovering_changed_count_);
  EXPECT_TRUE(observer.last_discovering_);
  EXPECT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)4, discovery_sessions_.size());
  EXPECT_TRUE(discovery_sessions_[3]->IsActive());
}

TEST_F(BluetoothChromeOSTest, StartDiscoverySession) {
  GetAdapter();

  adapter_->SetPowered(
      true,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(adapter_->IsPowered());
  callback_count_ = 0;

  TestObserver observer(adapter_);

  EXPECT_EQ(0, observer.discovering_changed_count_);
  EXPECT_FALSE(observer.last_discovering_);
  EXPECT_FALSE(adapter_->IsDiscovering());
  EXPECT_TRUE(discovery_sessions_.empty());

  // Request a new discovery session.
  adapter_->StartDiscoverySession(
      base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  message_loop_.Run();
  EXPECT_EQ(1, observer.discovering_changed_count_);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering_);
  EXPECT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)1, discovery_sessions_.size());
  EXPECT_TRUE(discovery_sessions_[0]->IsActive());

  // Start another session. A new one should be returned in the callback, which
  // in turn will destroy the previous session. Adapter should still be
  // discovering and the reference count should be 1.
  adapter_->StartDiscoverySession(
      base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  message_loop_.Run();
  EXPECT_EQ(1, observer.discovering_changed_count_);
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering_);
  EXPECT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)2, discovery_sessions_.size());
  EXPECT_TRUE(discovery_sessions_[0]->IsActive());

  // Request a new session.
  adapter_->StartDiscoverySession(
      base::Bind(&BluetoothChromeOSTest::DiscoverySessionCallback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  message_loop_.Run();
  EXPECT_EQ(1, observer.discovering_changed_count_);
  EXPECT_EQ(3, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering_);
  EXPECT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)3, discovery_sessions_.size());
  EXPECT_TRUE(discovery_sessions_[1]->IsActive());
  EXPECT_NE(discovery_sessions_[0], discovery_sessions_[1]);

  // Stop the previous discovery session. The session should end but discovery
  // should continue.
  discovery_sessions_[0]->Stop(
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  message_loop_.Run();
  EXPECT_EQ(1, observer.discovering_changed_count_);
  EXPECT_EQ(4, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_TRUE(observer.last_discovering_);
  EXPECT_TRUE(adapter_->IsDiscovering());
  ASSERT_EQ((size_t)3, discovery_sessions_.size());
  EXPECT_FALSE(discovery_sessions_[0]->IsActive());
  EXPECT_TRUE(discovery_sessions_[1]->IsActive());

  // Delete the current active session. Discovery should eventually stop.
  discovery_sessions_.clear();
  while (observer.last_discovering_)
    message_loop_.RunUntilIdle();

  EXPECT_EQ(2, observer.discovering_changed_count_);
  EXPECT_EQ(4, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_FALSE(observer.last_discovering_);
  EXPECT_FALSE(adapter_->IsDiscovering());
}

TEST_F(BluetoothChromeOSTest, DeviceProperties) {
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(1U, devices.size());
  ASSERT_EQ(FakeBluetoothDeviceClient::kPairedDeviceAddress,
            devices[0]->GetAddress());

  // Verify the other device properties.
  EXPECT_EQ(base::UTF8ToUTF16(FakeBluetoothDeviceClient::kPairedDeviceName),
            devices[0]->GetName());
  EXPECT_EQ(BluetoothDevice::DEVICE_COMPUTER, devices[0]->GetDeviceType());
  EXPECT_TRUE(devices[0]->IsPaired());
  EXPECT_FALSE(devices[0]->IsConnected());
  EXPECT_FALSE(devices[0]->IsConnecting());

  // Non HID devices are always connectable.
  EXPECT_TRUE(devices[0]->IsConnectable());

  BluetoothDevice::UUIDList uuids = devices[0]->GetUUIDs();
  ASSERT_EQ(2U, uuids.size());
  EXPECT_EQ(uuids[0], BluetoothUUID("1800"));
  EXPECT_EQ(uuids[1], BluetoothUUID("1801"));

  EXPECT_EQ(BluetoothDevice::VENDOR_ID_USB, devices[0]->GetVendorIDSource());
  EXPECT_EQ(0x05ac, devices[0]->GetVendorID());
  EXPECT_EQ(0x030d, devices[0]->GetProductID());
  EXPECT_EQ(0x0306, devices[0]->GetDeviceID());
}

TEST_F(BluetoothChromeOSTest, DeviceClassChanged) {
  // Simulate a change of class of a device, as sometimes occurs
  // during discovery.
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(1U, devices.size());
  ASSERT_EQ(FakeBluetoothDeviceClient::kPairedDeviceAddress,
            devices[0]->GetAddress());
  ASSERT_EQ(BluetoothDevice::DEVICE_COMPUTER, devices[0]->GetDeviceType());

  // Install an observer; expect the DeviceChanged method to be called when
  // we change the class of the device.
  TestObserver observer(adapter_);

  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kPairedDevicePath));

  properties->bluetooth_class.ReplaceValue(0x002580);

  EXPECT_EQ(1, observer.device_changed_count_);
  EXPECT_EQ(devices[0], observer.last_device_);

  EXPECT_EQ(BluetoothDevice::DEVICE_MOUSE, devices[0]->GetDeviceType());
}

TEST_F(BluetoothChromeOSTest, DeviceNameChanged) {
  // Simulate a change of name of a device.
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(1U, devices.size());
  ASSERT_EQ(FakeBluetoothDeviceClient::kPairedDeviceAddress,
            devices[0]->GetAddress());
  ASSERT_EQ(base::UTF8ToUTF16(FakeBluetoothDeviceClient::kPairedDeviceName),
            devices[0]->GetName());

  // Install an observer; expect the DeviceChanged method to be called when
  // we change the alias of the device.
  TestObserver observer(adapter_);

  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kPairedDevicePath));

  static const std::string new_name("New Device Name");
  properties->alias.ReplaceValue(new_name);

  EXPECT_EQ(1, observer.device_changed_count_);
  EXPECT_EQ(devices[0], observer.last_device_);

  EXPECT_EQ(base::UTF8ToUTF16(new_name), devices[0]->GetName());
}

TEST_F(BluetoothChromeOSTest, DeviceUuidsChanged) {
  // Simulate a change of advertised services of a device.
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(1U, devices.size());
  ASSERT_EQ(FakeBluetoothDeviceClient::kPairedDeviceAddress,
            devices[0]->GetAddress());

  BluetoothDevice::UUIDList uuids = devices[0]->GetUUIDs();
  ASSERT_EQ(2U, uuids.size());
  ASSERT_EQ(uuids[0], BluetoothUUID("1800"));
  ASSERT_EQ(uuids[1], BluetoothUUID("1801"));

  // Install an observer; expect the DeviceChanged method to be called when
  // we change the class of the device.
  TestObserver observer(adapter_);

  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kPairedDevicePath));

  std::vector<std::string> new_uuids;
  new_uuids.push_back(uuids[0].canonical_value());
  new_uuids.push_back(uuids[1].canonical_value());
  new_uuids.push_back("0000110c-0000-1000-8000-00805f9b34fb");
  new_uuids.push_back("0000110e-0000-1000-8000-00805f9b34fb");
  new_uuids.push_back("0000110a-0000-1000-8000-00805f9b34fb");

  properties->uuids.ReplaceValue(new_uuids);

  EXPECT_EQ(1, observer.device_changed_count_);
  EXPECT_EQ(devices[0], observer.last_device_);

  // Fetching the value should give the new one.
  uuids = devices[0]->GetUUIDs();
  ASSERT_EQ(5U, uuids.size());
  EXPECT_EQ(uuids[0], BluetoothUUID("1800"));
  EXPECT_EQ(uuids[1], BluetoothUUID("1801"));
  EXPECT_EQ(uuids[2], BluetoothUUID("110c"));
  EXPECT_EQ(uuids[3], BluetoothUUID("110e"));
  EXPECT_EQ(uuids[4], BluetoothUUID("110a"));
}

TEST_F(BluetoothChromeOSTest, ForgetDevice) {
  GetAdapter();

  BluetoothAdapter::DeviceList devices = adapter_->GetDevices();
  ASSERT_EQ(1U, devices.size());
  ASSERT_EQ(FakeBluetoothDeviceClient::kPairedDeviceAddress,
            devices[0]->GetAddress());

  std::string address = devices[0]->GetAddress();

  // Install an observer; expect the DeviceRemoved method to be called
  // with the device we remove.
  TestObserver observer(adapter_);

  devices[0]->Forget(
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.device_removed_count_);
  EXPECT_EQ(address, observer.last_device_address_);

  // GetDevices shouldn't return the device either.
  devices = adapter_->GetDevices();
  ASSERT_EQ(0U, devices.size());
}

TEST_F(BluetoothChromeOSTest, ForgetUnpairedDevice) {
  GetAdapter();
  DiscoverDevices();

  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kConnectUnpairableAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  // Connect the device so it becomes trusted and remembered.
  device->Connect(
      NULL,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  ASSERT_EQ(1, callback_count_);
  ASSERT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  ASSERT_TRUE(device->IsConnected());
  ASSERT_FALSE(device->IsConnecting());

  // Make sure the trusted property has been set to true.
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kConnectUnpairablePath));
  ASSERT_TRUE(properties->trusted.value());

  // Install an observer; expect the DeviceRemoved method to be called
  // with the device we remove.
  TestObserver observer(adapter_);

  device->Forget(
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.device_removed_count_);
  EXPECT_EQ(FakeBluetoothDeviceClient::kConnectUnpairableAddress,
            observer.last_device_address_);

  // GetDevices shouldn't return the device either.
  device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kConnectUnpairableAddress);
  EXPECT_FALSE(device != NULL);
}

TEST_F(BluetoothChromeOSTest, ConnectPairedDevice) {
  GetAdapter();

  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_TRUE(device->IsPaired());

  TestObserver observer(adapter_);

  // Connect without a pairing delegate; since the device is already Paired
  // this should succeed and the device should become connected.
  device->Connect(
      NULL,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Two changes for connecting, one for connected and one for for trusted
  // after connecting.
  EXPECT_EQ(4, observer.device_changed_count_);
  EXPECT_EQ(device, observer.last_device_);

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
}

TEST_F(BluetoothChromeOSTest, ConnectUnpairableDevice) {
  GetAdapter();
  DiscoverDevices();

  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kConnectUnpairableAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);

  // Connect without a pairing delegate; since the device does not require
  // pairing, this should succeed and the device should become connected.
  device->Connect(
      NULL,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Two changes for connecting, one for connected, one for for trusted after
  // connection, and one for the reconnect mode (IsConnectable).
  EXPECT_EQ(5, observer.device_changed_count_);
  EXPECT_EQ(device, observer.last_device_);

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  // Make sure the trusted property has been set to true.
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kConnectUnpairablePath));
  EXPECT_TRUE(properties->trusted.value());

  // Verify is a HID device and is not connectable.
  BluetoothDevice::UUIDList uuids = device->GetUUIDs();
  ASSERT_EQ(1U, uuids.size());
  EXPECT_EQ(uuids[0], BluetoothUUID("1124"));
  EXPECT_FALSE(device->IsConnectable());
}

TEST_F(BluetoothChromeOSTest, ConnectConnectedDevice) {
  GetAdapter();

  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_TRUE(device->IsPaired());

  device->Connect(
      NULL,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  ASSERT_EQ(1, callback_count_);
  ASSERT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  ASSERT_TRUE(device->IsConnected());

  // Connect again; since the device is already Connected, this shouldn't do
  // anything to initiate the connection.
  TestObserver observer(adapter_);

  device->Connect(
      NULL,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // The observer will be called because Connecting will toggle true and false,
  // and the trusted property will be updated to true.
  EXPECT_EQ(3, observer.device_changed_count_);

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
}

TEST_F(BluetoothChromeOSTest, ConnectDeviceFails) {
  GetAdapter();
  DiscoverDevices();

  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kLegacyAutopairAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);

  // Connect without a pairing delegate; since the device requires pairing,
  // this should fail with an error.
  device->Connect(
      NULL,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothDevice::ERROR_FAILED, last_connect_error_);

  EXPECT_EQ(2, observer.device_changed_count_);

  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
}

TEST_F(BluetoothChromeOSTest, DisconnectDevice) {
  GetAdapter();

  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_TRUE(device->IsPaired());

  device->Connect(
      NULL,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  ASSERT_EQ(1, callback_count_);
  ASSERT_EQ(0, error_callback_count_);
  callback_count_ = 0;

  ASSERT_TRUE(device->IsConnected());
  ASSERT_FALSE(device->IsConnecting());

  // Disconnect the device, we should see the observer method fire and the
  // device get dropped.
  TestObserver observer(adapter_);

  device->Disconnect(
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  EXPECT_EQ(1, observer.device_changed_count_);
  EXPECT_EQ(device, observer.last_device_);

  EXPECT_FALSE(device->IsConnected());
}

TEST_F(BluetoothChromeOSTest, DisconnectUnconnectedDevice) {
  GetAdapter();

  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kPairedDeviceAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_TRUE(device->IsPaired());
  ASSERT_FALSE(device->IsConnected());

  // Disconnect the device, we should see the observer method fire and the
  // device get dropped.
  TestObserver observer(adapter_);

  device->Disconnect(
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);

  EXPECT_EQ(0, observer.device_changed_count_);

  EXPECT_FALSE(device->IsConnected());
}

TEST_F(BluetoothChromeOSTest, PairLegacyAutopair) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // The Legacy Autopair device requires no PIN or Passkey to pair because
  // the daemon provides 0000 to the device for us.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kLegacyAutopairAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(
      &pairing_delegate,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(0, pairing_delegate.call_count_);
  EXPECT_TRUE(device->IsConnecting());

  message_loop_.Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Two changes for connecting, one change for connected, one for paired,
  // two for trusted (after pairing and connection), and one for the reconnect
  // mode (IsConnectable).
  EXPECT_EQ(7, observer.device_changed_count_);
  EXPECT_EQ(device, observer.last_device_);

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  EXPECT_TRUE(device->IsPaired());

  // Verify is a HID device and is connectable.
  BluetoothDevice::UUIDList uuids = device->GetUUIDs();
  ASSERT_EQ(1U, uuids.size());
  EXPECT_EQ(uuids[0], BluetoothUUID("1124"));
  EXPECT_TRUE(device->IsConnectable());

  // Make sure the trusted property has been set to true.
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kLegacyAutopairPath));
  EXPECT_TRUE(properties->trusted.value());
}

TEST_F(BluetoothChromeOSTest, PairDisplayPinCode) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Requires that we display a randomly generated PIN on the screen.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kDisplayPinCodeAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(
      &pairing_delegate,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.display_pincode_count_);
  EXPECT_EQ("123456", pairing_delegate.last_pincode_);
  EXPECT_TRUE(device->IsConnecting());

  message_loop_.Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Two changes for connecting, one change for connected, one for paired,
  // two for trusted (after pairing and connection), and one for the reconnect
  // mode (IsConnectable).
  EXPECT_EQ(7, observer.device_changed_count_);
  EXPECT_EQ(device, observer.last_device_);

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  EXPECT_TRUE(device->IsPaired());

  // Verify is a HID device and is connectable.
  BluetoothDevice::UUIDList uuids = device->GetUUIDs();
  ASSERT_EQ(1U, uuids.size());
  EXPECT_EQ(uuids[0], BluetoothUUID("1124"));
  EXPECT_TRUE(device->IsConnectable());

  // Make sure the trusted property has been set to true.
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kDisplayPinCodePath));
  EXPECT_TRUE(properties->trusted.value());
}

TEST_F(BluetoothChromeOSTest, PairDisplayPasskey) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Requires that we display a randomly generated Passkey on the screen,
  // and notifies us as it's typed in.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kDisplayPasskeyAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(
      &pairing_delegate,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  // One call for DisplayPasskey() and one for KeysEntered().
  EXPECT_EQ(2, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.display_passkey_count_);
  EXPECT_EQ(123456U, pairing_delegate.last_passkey_);
  EXPECT_EQ(1, pairing_delegate.keys_entered_count_);
  EXPECT_EQ(0U, pairing_delegate.last_entered_);

  EXPECT_TRUE(device->IsConnecting());

  // One call to KeysEntered() for each key, including [enter].
  for(int i = 1; i <= 7; ++i) {
    message_loop_.Run();

    EXPECT_EQ(2 + i, pairing_delegate.call_count_);
    EXPECT_EQ(1 + i, pairing_delegate.keys_entered_count_);
    EXPECT_EQ(static_cast<uint32_t>(i), pairing_delegate.last_entered_);
  }

  message_loop_.Run();

  // 8 KeysEntered notifications (0 to 7, inclusive) and one aditional call for
  // DisplayPasskey().
  EXPECT_EQ(9, pairing_delegate.call_count_);
  EXPECT_EQ(8, pairing_delegate.keys_entered_count_);
  EXPECT_EQ(7U, pairing_delegate.last_entered_);

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Two changes for connecting, one change for connected, one for paired,
  // two for trusted (after pairing and connection), and one for the reconnect
  // mode (IsConnectable).
  EXPECT_EQ(7, observer.device_changed_count_);
  EXPECT_EQ(device, observer.last_device_);

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  EXPECT_TRUE(device->IsPaired());

  // Verify is a HID device.
  BluetoothDevice::UUIDList uuids = device->GetUUIDs();
  ASSERT_EQ(1U, uuids.size());
  EXPECT_EQ(uuids[0], BluetoothUUID("1124"));

  // And usually not connectable.
  EXPECT_FALSE(device->IsConnectable());

  // Make sure the trusted property has been set to true.
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kDisplayPasskeyPath));
  EXPECT_TRUE(properties->trusted.value());
}

TEST_F(BluetoothChromeOSTest, PairRequestPinCode) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Requires that the user enters a PIN for them.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kRequestPinCodeAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(
      &pairing_delegate,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.request_pincode_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Set the PIN.
  device->SetPinCode("1234");
  message_loop_.Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Two changes for connecting, one change for connected, one for paired and
  // two for trusted (after pairing and connection).
  EXPECT_EQ(6, observer.device_changed_count_);
  EXPECT_EQ(device, observer.last_device_);

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  EXPECT_TRUE(device->IsPaired());

  // Verify is not a HID device.
  BluetoothDevice::UUIDList uuids = device->GetUUIDs();
  ASSERT_EQ(0U, uuids.size());

  // Non HID devices are always connectable.
  EXPECT_TRUE(device->IsConnectable());

  // Make sure the trusted property has been set to true.
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kRequestPinCodePath));
  EXPECT_TRUE(properties->trusted.value());
}

TEST_F(BluetoothChromeOSTest, PairConfirmPasskey) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Requests that we confirm a displayed passkey.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kConfirmPasskeyAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(
      &pairing_delegate,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.confirm_passkey_count_);
  EXPECT_EQ(123456U, pairing_delegate.last_passkey_);
  EXPECT_TRUE(device->IsConnecting());

  // Confirm the passkey.
  device->ConfirmPairing();
  message_loop_.Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Two changes for connecting, one change for connected, one for paired and
  // two for trusted (after pairing and connection).
  EXPECT_EQ(6, observer.device_changed_count_);
  EXPECT_EQ(device, observer.last_device_);

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  EXPECT_TRUE(device->IsPaired());

  // Non HID devices are always connectable.
  EXPECT_TRUE(device->IsConnectable());

  // Make sure the trusted property has been set to true.
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kConfirmPasskeyPath));
  EXPECT_TRUE(properties->trusted.value());
}

TEST_F(BluetoothChromeOSTest, PairRequestPasskey) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Requires that the user enters a Passkey, this would be some kind of
  // device that has a display, but doesn't use "just works" - maybe a car?
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kRequestPasskeyAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(
      &pairing_delegate,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.request_passkey_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Set the Passkey.
  device->SetPasskey(1234);
  message_loop_.Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Two changes for connecting, one change for connected, one for paired and
  // two for trusted (after pairing and connection).
  EXPECT_EQ(6, observer.device_changed_count_);
  EXPECT_EQ(device, observer.last_device_);

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  EXPECT_TRUE(device->IsPaired());

  // Non HID devices are always connectable.
  EXPECT_TRUE(device->IsConnectable());

  // Make sure the trusted property has been set to true.
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kRequestPasskeyPath));
  EXPECT_TRUE(properties->trusted.value());
}

TEST_F(BluetoothChromeOSTest, PairJustWorks) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Uses just-works pairing, since this is an outgoing pairing, no delegate
  // interaction is required.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kJustWorksAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(
      &pairing_delegate,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(0, pairing_delegate.call_count_);

  message_loop_.Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // Two changes for connecting, one change for connected, one for paired and
  // two for trusted (after pairing and connection).
  EXPECT_EQ(6, observer.device_changed_count_);
  EXPECT_EQ(device, observer.last_device_);

  EXPECT_TRUE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  EXPECT_TRUE(device->IsPaired());

  // Non HID devices are always connectable.
  EXPECT_TRUE(device->IsConnectable());

  // Make sure the trusted property has been set to true.
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kJustWorksPath));
  EXPECT_TRUE(properties->trusted.value());
}

TEST_F(BluetoothChromeOSTest, PairUnpairableDeviceFails) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevice(FakeBluetoothDeviceClient::kUnconnectableDeviceAddress);

  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kUnpairableDeviceAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(
      &pairing_delegate,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(0, pairing_delegate.call_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Run the loop to get the error..
  message_loop_.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);

  EXPECT_EQ(BluetoothDevice::ERROR_FAILED, last_connect_error_);

  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());
}

TEST_F(BluetoothChromeOSTest, PairingFails) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevice(FakeBluetoothDeviceClient::kVanishingDeviceAddress);

  // The vanishing device times out during pairing
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kVanishingDeviceAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(
      &pairing_delegate,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(0, pairing_delegate.call_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Run the loop to get the error..
  message_loop_.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);

  EXPECT_EQ(BluetoothDevice::ERROR_AUTH_TIMEOUT, last_connect_error_);

  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());
}

TEST_F(BluetoothChromeOSTest, PairingFailsAtConnection) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Everything seems to go according to plan with the unconnectable device;
  // it pairs, but then you can't make connections to it after.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kUnconnectableDeviceAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(
      &pairing_delegate,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(0, pairing_delegate.call_count_);
  EXPECT_TRUE(device->IsConnecting());

  message_loop_.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothDevice::ERROR_FAILED, last_connect_error_);

  // Two changes for connecting, one for paired and one for trusted after
  // pairing. The device should not be connected.
  EXPECT_EQ(4, observer.device_changed_count_);
  EXPECT_EQ(device, observer.last_device_);

  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());

  EXPECT_TRUE(device->IsPaired());

  // Make sure the trusted property has been set to true still (since pairing
  // worked).
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(
              FakeBluetoothDeviceClient::kUnconnectableDevicePath));
  EXPECT_TRUE(properties->trusted.value());
}

TEST_F(BluetoothChromeOSTest, PairingRejectedAtPinCode) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Reject the pairing after we receive a request for the PIN code.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kRequestPinCodeAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(
      &pairing_delegate,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.request_pincode_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Reject the pairing.
  device->RejectPairing();
  message_loop_.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothDevice::ERROR_AUTH_REJECTED, last_connect_error_);

  // Should be no changes except connecting going true and false.
  EXPECT_EQ(2, observer.device_changed_count_);
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());
}

TEST_F(BluetoothChromeOSTest, PairingCancelledAtPinCode) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Cancel the pairing after we receive a request for the PIN code.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kRequestPinCodeAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(
      &pairing_delegate,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.request_pincode_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Cancel the pairing.
  device->CancelPairing();
  message_loop_.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothDevice::ERROR_AUTH_CANCELED, last_connect_error_);

  // Should be no changes except connecting going true and false.
  EXPECT_EQ(2, observer.device_changed_count_);
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());
}

TEST_F(BluetoothChromeOSTest, PairingRejectedAtPasskey) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Reject the pairing after we receive a request for the passkey.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kRequestPasskeyAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(
      &pairing_delegate,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.request_passkey_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Reject the pairing.
  device->RejectPairing();
  message_loop_.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothDevice::ERROR_AUTH_REJECTED, last_connect_error_);

  // Should be no changes except connecting going true and false.
  EXPECT_EQ(2, observer.device_changed_count_);
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());
}

TEST_F(BluetoothChromeOSTest, PairingCancelledAtPasskey) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Cancel the pairing after we receive a request for the passkey.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kRequestPasskeyAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(
      &pairing_delegate,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.request_passkey_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Cancel the pairing.
  device->CancelPairing();
  message_loop_.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothDevice::ERROR_AUTH_CANCELED, last_connect_error_);

  // Should be no changes except connecting going true and false.
  EXPECT_EQ(2, observer.device_changed_count_);
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());
}

TEST_F(BluetoothChromeOSTest, PairingRejectedAtConfirmation) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Reject the pairing after we receive a request for passkey confirmation.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kConfirmPasskeyAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(
      &pairing_delegate,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.confirm_passkey_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Reject the pairing.
  device->RejectPairing();
  message_loop_.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothDevice::ERROR_AUTH_REJECTED, last_connect_error_);

  // Should be no changes except connecting going true and false.
  EXPECT_EQ(2, observer.device_changed_count_);
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());
}

TEST_F(BluetoothChromeOSTest, PairingCancelledAtConfirmation) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Cancel the pairing after we receive a request for the passkey.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kConfirmPasskeyAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(
      &pairing_delegate,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.confirm_passkey_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Cancel the pairing.
  device->CancelPairing();
  message_loop_.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothDevice::ERROR_AUTH_CANCELED, last_connect_error_);

  // Should be no changes except connecting going true and false.
  EXPECT_EQ(2, observer.device_changed_count_);
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());
}

TEST_F(BluetoothChromeOSTest, PairingCancelledInFlight) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();
  DiscoverDevices();

  // Cancel the pairing while we're waiting for the remote host.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kLegacyAutopairAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);

  TestPairingDelegate pairing_delegate;
  device->Connect(
      &pairing_delegate,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::ConnectErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(0, pairing_delegate.call_count_);
  EXPECT_TRUE(device->IsConnecting());

  // Cancel the pairing.
  device->CancelPairing();
  message_loop_.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(BluetoothDevice::ERROR_AUTH_CANCELED, last_connect_error_);

  // Should be no changes except connecting going true and false.
  EXPECT_EQ(2, observer.device_changed_count_);
  EXPECT_FALSE(device->IsConnected());
  EXPECT_FALSE(device->IsConnecting());
  EXPECT_FALSE(device->IsPaired());
}

TEST_F(BluetoothChromeOSTest, IncomingPairRequestPinCode) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();

  TestPairingDelegate pairing_delegate;
  adapter_->AddPairingDelegate(
      &pairing_delegate,
      BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH);

  // Requires that we provide a PIN code.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kRequestPinCodePath));
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kRequestPinCodeAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);

  fake_bluetooth_device_client_->SimulatePairing(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kRequestPinCodePath),
      true,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::DBusErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.request_pincode_count_);

  // Set the PIN.
  device->SetPinCode("1234");
  message_loop_.Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // One change for paired, and one for trusted.
  EXPECT_EQ(2, observer.device_changed_count_);
  EXPECT_EQ(device, observer.last_device_);

  EXPECT_TRUE(device->IsPaired());

  // Make sure the trusted property has been set to true.
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kRequestPinCodePath));
  ASSERT_TRUE(properties->trusted.value());

  // No pairing context should remain on the device.
  BluetoothDeviceChromeOS* device_chromeos =
      static_cast<BluetoothDeviceChromeOS*>(device);
  EXPECT_TRUE(device_chromeos->GetPairing() == NULL);
}

TEST_F(BluetoothChromeOSTest, IncomingPairConfirmPasskey) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();

  TestPairingDelegate pairing_delegate;
  adapter_->AddPairingDelegate(
      &pairing_delegate,
      BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH);

  // Requests that we confirm a displayed passkey.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kConfirmPasskeyPath));
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kConfirmPasskeyAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);

  fake_bluetooth_device_client_->SimulatePairing(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kConfirmPasskeyPath),
      true,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::DBusErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.confirm_passkey_count_);
  EXPECT_EQ(123456U, pairing_delegate.last_passkey_);

  // Confirm the passkey.
  device->ConfirmPairing();
  message_loop_.Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // One change for paired, and one for trusted.
  EXPECT_EQ(2, observer.device_changed_count_);
  EXPECT_EQ(device, observer.last_device_);

  EXPECT_TRUE(device->IsPaired());

  // Make sure the trusted property has been set to true.
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kConfirmPasskeyPath));
  ASSERT_TRUE(properties->trusted.value());

  // No pairing context should remain on the device.
  BluetoothDeviceChromeOS* device_chromeos =
      static_cast<BluetoothDeviceChromeOS*>(device);
  EXPECT_TRUE(device_chromeos->GetPairing() == NULL);
}

TEST_F(BluetoothChromeOSTest, IncomingPairRequestPasskey) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();

  TestPairingDelegate pairing_delegate;
  adapter_->AddPairingDelegate(
      &pairing_delegate,
      BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH);

  // Requests that we provide a Passkey.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kRequestPasskeyPath));
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kRequestPasskeyAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);

  fake_bluetooth_device_client_->SimulatePairing(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kRequestPasskeyPath),
      true,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::DBusErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.request_passkey_count_);

  // Set the Passkey.
  device->SetPasskey(1234);
  message_loop_.Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // One change for paired, and one for trusted.
  EXPECT_EQ(2, observer.device_changed_count_);
  EXPECT_EQ(device, observer.last_device_);

  EXPECT_TRUE(device->IsPaired());

  // Make sure the trusted property has been set to true.
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kRequestPasskeyPath));
  ASSERT_TRUE(properties->trusted.value());

  // No pairing context should remain on the device.
  BluetoothDeviceChromeOS* device_chromeos =
      static_cast<BluetoothDeviceChromeOS*>(device);
  EXPECT_TRUE(device_chromeos->GetPairing() == NULL);
}

TEST_F(BluetoothChromeOSTest, IncomingPairJustWorks) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();

  TestPairingDelegate pairing_delegate;
  adapter_->AddPairingDelegate(
      &pairing_delegate,
      BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH);

  // Uses just-works pairing so, sinec this an incoming pairing, require
  // authorization from the user.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kJustWorksPath));
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kJustWorksAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);

  fake_bluetooth_device_client_->SimulatePairing(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kJustWorksPath),
      true,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::DBusErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.authorize_pairing_count_);

  // Confirm the pairing.
  device->ConfirmPairing();
  message_loop_.Run();

  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(0, error_callback_count_);

  // One change for paired, and one for trusted.
  EXPECT_EQ(2, observer.device_changed_count_);
  EXPECT_EQ(device, observer.last_device_);

  EXPECT_TRUE(device->IsPaired());

  // Make sure the trusted property has been set to true.
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kJustWorksPath));
  ASSERT_TRUE(properties->trusted.value());

  // No pairing context should remain on the device.
  BluetoothDeviceChromeOS* device_chromeos =
      static_cast<BluetoothDeviceChromeOS*>(device);
  EXPECT_TRUE(device_chromeos->GetPairing() == NULL);
}

TEST_F(BluetoothChromeOSTest, IncomingPairRequestPinCodeWithoutDelegate) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();

  // Requires that we provide a PIN Code, without a pairing delegate,
  // that will be rejected.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kRequestPinCodePath));
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kRequestPinCodeAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);

  fake_bluetooth_device_client_->SimulatePairing(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kRequestPinCodePath),
      true,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::DBusErrorCallback,
                 base::Unretained(this)));

  message_loop_.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(bluetooth_device::kErrorAuthenticationRejected, last_client_error_);

  // No changes should be observer.
  EXPECT_EQ(0, observer.device_changed_count_);

  EXPECT_FALSE(device->IsPaired());

  // No pairing context should remain on the device.
  BluetoothDeviceChromeOS* device_chromeos =
      static_cast<BluetoothDeviceChromeOS*>(device);
  EXPECT_TRUE(device_chromeos->GetPairing() == NULL);
}

TEST_F(BluetoothChromeOSTest, IncomingPairConfirmPasskeyWithoutDelegate) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();

  // Requests that we confirm a displayed passkey, without a pairing delegate,
  // that will be rejected.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kConfirmPasskeyPath));
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kConfirmPasskeyAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);

  fake_bluetooth_device_client_->SimulatePairing(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kConfirmPasskeyPath),
      true,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::DBusErrorCallback,
                 base::Unretained(this)));

  message_loop_.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(bluetooth_device::kErrorAuthenticationRejected, last_client_error_);

  // No changes should be observer.
  EXPECT_EQ(0, observer.device_changed_count_);

  EXPECT_FALSE(device->IsPaired());

  // No pairing context should remain on the device.
  BluetoothDeviceChromeOS* device_chromeos =
      static_cast<BluetoothDeviceChromeOS*>(device);
  EXPECT_TRUE(device_chromeos->GetPairing() == NULL);
}

TEST_F(BluetoothChromeOSTest, IncomingPairRequestPasskeyWithoutDelegate) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();

  // Requests that we provide a displayed passkey, without a pairing delegate,
  // that will be rejected.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kRequestPasskeyPath));
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kRequestPasskeyAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);

  fake_bluetooth_device_client_->SimulatePairing(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kRequestPasskeyPath),
      true,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::DBusErrorCallback,
                 base::Unretained(this)));

  message_loop_.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(bluetooth_device::kErrorAuthenticationRejected, last_client_error_);

  // No changes should be observer.
  EXPECT_EQ(0, observer.device_changed_count_);

  EXPECT_FALSE(device->IsPaired());

  // No pairing context should remain on the device.
  BluetoothDeviceChromeOS* device_chromeos =
      static_cast<BluetoothDeviceChromeOS*>(device);
  EXPECT_TRUE(device_chromeos->GetPairing() == NULL);
}

TEST_F(BluetoothChromeOSTest, IncomingPairJustWorksWithoutDelegate) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();

  // Uses just-works pairing and thus requires authorization for incoming
  // pairings, without a pairing delegate, that will be rejected.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kJustWorksPath));
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kJustWorksAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);

  fake_bluetooth_device_client_->SimulatePairing(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kJustWorksPath),
      true,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::DBusErrorCallback,
                 base::Unretained(this)));

  message_loop_.Run();

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(1, error_callback_count_);
  EXPECT_EQ(bluetooth_device::kErrorAuthenticationRejected, last_client_error_);

  // No changes should be observer.
  EXPECT_EQ(0, observer.device_changed_count_);

  EXPECT_FALSE(device->IsPaired());

  // No pairing context should remain on the device.
  BluetoothDeviceChromeOS* device_chromeos =
      static_cast<BluetoothDeviceChromeOS*>(device);
  EXPECT_TRUE(device_chromeos->GetPairing() == NULL);
}

TEST_F(BluetoothChromeOSTest, RemovePairingDelegateDuringPairing) {
  fake_bluetooth_device_client_->SetSimulationIntervalMs(10);

  GetAdapter();

  TestPairingDelegate pairing_delegate;
  adapter_->AddPairingDelegate(
      &pairing_delegate,
      BluetoothAdapter::PAIRING_DELEGATE_PRIORITY_HIGH);

  // Requests that we provide a Passkey.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kRequestPasskeyPath));
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kRequestPasskeyAddress);
  ASSERT_TRUE(device != NULL);
  ASSERT_FALSE(device->IsPaired());

  TestObserver observer(adapter_);

  fake_bluetooth_device_client_->SimulatePairing(
      dbus::ObjectPath(FakeBluetoothDeviceClient::kRequestPasskeyPath),
      true,
      base::Bind(&BluetoothChromeOSTest::Callback,
                 base::Unretained(this)),
      base::Bind(&BluetoothChromeOSTest::DBusErrorCallback,
                 base::Unretained(this)));

  EXPECT_EQ(1, pairing_delegate.call_count_);
  EXPECT_EQ(1, pairing_delegate.request_passkey_count_);

  // A pairing context should now be set on the device.
  BluetoothDeviceChromeOS* device_chromeos =
      static_cast<BluetoothDeviceChromeOS*>(device);
  ASSERT_TRUE(device_chromeos->GetPairing() != NULL);

  // Removing the pairing delegate should remove that pairing context.
  adapter_->RemovePairingDelegate(&pairing_delegate);

  EXPECT_TRUE(device_chromeos->GetPairing() == NULL);

  // Set the Passkey, this should now have no effect since the pairing has
  // been, in-effect, cancelled
  device->SetPasskey(1234);

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(0, error_callback_count_);
  EXPECT_EQ(0, observer.device_changed_count_);

  EXPECT_FALSE(device->IsPaired());
}

TEST_F(BluetoothChromeOSTest, DeviceId) {
  GetAdapter();

  // Use the built-in paired device for this test, grab its Properties
  // structure so we can adjust the underlying modalias property.
  BluetoothDevice* device = adapter_->GetDevice(
      FakeBluetoothDeviceClient::kPairedDeviceAddress);
  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kPairedDevicePath));

  ASSERT_TRUE(device != NULL);
  ASSERT_TRUE(properties != NULL);

  // Valid USB IF-assigned identifier.
  ASSERT_EQ("usb:v05ACp030Dd0306", properties->modalias.value());

  EXPECT_EQ(BluetoothDevice::VENDOR_ID_USB, device->GetVendorIDSource());
  EXPECT_EQ(0x05ac, device->GetVendorID());
  EXPECT_EQ(0x030d, device->GetProductID());
  EXPECT_EQ(0x0306, device->GetDeviceID());

  // Valid Bluetooth SIG-assigned identifier.
  properties->modalias.ReplaceValue("bluetooth:v00E0p2400d0400");

  EXPECT_EQ(BluetoothDevice::VENDOR_ID_BLUETOOTH, device->GetVendorIDSource());
  EXPECT_EQ(0x00e0, device->GetVendorID());
  EXPECT_EQ(0x2400, device->GetProductID());
  EXPECT_EQ(0x0400, device->GetDeviceID());

  // Invalid USB IF-assigned identifier.
  properties->modalias.ReplaceValue("usb:x00E0p2400d0400");

  EXPECT_EQ(BluetoothDevice::VENDOR_ID_UNKNOWN, device->GetVendorIDSource());
  EXPECT_EQ(0, device->GetVendorID());
  EXPECT_EQ(0, device->GetProductID());
  EXPECT_EQ(0, device->GetDeviceID());

  // Invalid Bluetooth SIG-assigned identifier.
  properties->modalias.ReplaceValue("bluetooth:x00E0p2400d0400");

  EXPECT_EQ(BluetoothDevice::VENDOR_ID_UNKNOWN, device->GetVendorIDSource());
  EXPECT_EQ(0, device->GetVendorID());
  EXPECT_EQ(0, device->GetProductID());
  EXPECT_EQ(0, device->GetDeviceID());

  // Unknown vendor specification identifier.
  properties->modalias.ReplaceValue("chrome:v00E0p2400d0400");

  EXPECT_EQ(BluetoothDevice::VENDOR_ID_UNKNOWN, device->GetVendorIDSource());
  EXPECT_EQ(0, device->GetVendorID());
  EXPECT_EQ(0, device->GetProductID());
  EXPECT_EQ(0, device->GetDeviceID());
}

}  // namespace chromeos
