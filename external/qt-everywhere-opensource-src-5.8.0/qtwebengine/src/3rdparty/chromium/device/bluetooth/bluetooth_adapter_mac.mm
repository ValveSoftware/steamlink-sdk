// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter_mac.h"

#import <IOBluetooth/objc/IOBluetoothDevice.h>
#import <IOBluetooth/objc/IOBluetoothHostController.h>
#include <stddef.h>

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/location.h"
#include "base/mac/mac_util.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/memory/ptr_util.h"
#include "base/profiler/scoped_tracker.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "device/bluetooth/bluetooth_classic_device_mac.h"
#include "device/bluetooth/bluetooth_common.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_discovery_session_outcome.h"
#include "device/bluetooth/bluetooth_low_energy_central_manager_delegate.h"
#include "device/bluetooth/bluetooth_socket_mac.h"

namespace {

// The frequency with which to poll the adapter for updates.
const int kPollIntervalMs = 500;

}  // namespace

namespace device {

// static
base::WeakPtr<BluetoothAdapter> BluetoothAdapter::CreateAdapter(
    const InitCallback& init_callback) {
  return BluetoothAdapterMac::CreateAdapter();
}

// static
base::WeakPtr<BluetoothAdapterMac> BluetoothAdapterMac::CreateAdapter() {
  BluetoothAdapterMac* adapter = new BluetoothAdapterMac();
  adapter->Init();
  return adapter->weak_ptr_factory_.GetWeakPtr();
}

// static
base::WeakPtr<BluetoothAdapterMac> BluetoothAdapterMac::CreateAdapterForTest(
    std::string name,
    std::string address,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner) {
  BluetoothAdapterMac* adapter = new BluetoothAdapterMac();
  adapter->InitForTest(ui_task_runner);
  adapter->name_ = name;
  adapter->address_ = address;
  return adapter->weak_ptr_factory_.GetWeakPtr();
}

// static
BluetoothUUID BluetoothAdapterMac::BluetoothUUIDWithCBUUID(CBUUID* uuid) {
  // UUIDString only available OS X >= 10.10.
  DCHECK(base::mac::IsOSYosemiteOrLater());
  std::string uuid_c_string = base::SysNSStringToUTF8([uuid UUIDString]);
  return device::BluetoothUUID(uuid_c_string);
}

BluetoothAdapterMac::BluetoothAdapterMac()
    : BluetoothAdapter(),
      classic_powered_(false),
      num_discovery_sessions_(0),
      classic_discovery_manager_(
          BluetoothDiscoveryManagerMac::CreateClassic(this)),
      weak_ptr_factory_(this) {
  if (IsLowEnergyAvailable()) {
    low_energy_discovery_manager_.reset(
        BluetoothLowEnergyDiscoveryManagerMac::Create(this));
    low_energy_central_manager_delegate_.reset(
        [[BluetoothLowEnergyCentralManagerDelegate alloc]
            initWithDiscoveryManager:low_energy_discovery_manager_.get()
                          andAdapter:this]);
    low_energy_central_manager_.reset([[CBCentralManager alloc]
        initWithDelegate:low_energy_central_manager_delegate_.get()
                   queue:dispatch_get_main_queue()]);
    low_energy_discovery_manager_->SetCentralManager(
        low_energy_central_manager_.get());
  }
  DCHECK(classic_discovery_manager_.get());
}

BluetoothAdapterMac::~BluetoothAdapterMac() {
  // When devices will be destroyed, they will need this current instance to
  // disconnect the gatt connection. To make sure they don't use the mac
  // adapter, they should be explicitly destroyed here.
  devices_.clear();
  // Set low_energy_central_manager_ to nil so no devices will try to use it
  // while being destroyed after this method. |devices_| is owned by
  // BluetoothAdapter.
  low_energy_central_manager_.reset();
}

std::string BluetoothAdapterMac::GetAddress() const {
  return address_;
}

std::string BluetoothAdapterMac::GetName() const {
  return name_;
}

void BluetoothAdapterMac::SetName(const std::string& name,
                                  const base::Closure& callback,
                                  const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

bool BluetoothAdapterMac::IsInitialized() const {
  return true;
}

bool BluetoothAdapterMac::IsPresent() const {
  bool is_present = !address_.empty();
  if (IsLowEnergyAvailable()) {
    is_present = is_present || ([low_energy_central_manager_ state] ==
                                CBCentralManagerStatePoweredOn);
  }
  return is_present;
}

bool BluetoothAdapterMac::IsPowered() const {
  bool is_powered = classic_powered_;
  if (IsLowEnergyAvailable()) {
    is_powered = is_powered || ([low_energy_central_manager_ state] ==
                                CBCentralManagerStatePoweredOn);
  }
  return is_powered;
}

void BluetoothAdapterMac::SetPowered(bool powered,
                                     const base::Closure& callback,
                                     const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

// TODO(krstnmnlsn): If this information is retrievable form IOBluetooth we
// should return the discoverable status.
bool BluetoothAdapterMac::IsDiscoverable() const {
  return false;
}

void BluetoothAdapterMac::SetDiscoverable(
    bool discoverable,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

bool BluetoothAdapterMac::IsDiscovering() const {
  bool is_discovering = classic_discovery_manager_->IsDiscovering();
  if (IsLowEnergyAvailable())
    is_discovering =
        is_discovering || low_energy_discovery_manager_->IsDiscovering();
  return is_discovering;
}

BluetoothAdapter::UUIDList BluetoothAdapterMac::GetUUIDs() const {
  NOTIMPLEMENTED();
  return UUIDList();
}

void BluetoothAdapterMac::CreateRfcommService(
    const BluetoothUUID& uuid,
    const ServiceOptions& options,
    const CreateServiceCallback& callback,
    const CreateServiceErrorCallback& error_callback) {
  scoped_refptr<BluetoothSocketMac> socket = BluetoothSocketMac::CreateSocket();
  socket->ListenUsingRfcomm(
      this, uuid, options, base::Bind(callback, socket), error_callback);
}

void BluetoothAdapterMac::CreateL2capService(
    const BluetoothUUID& uuid,
    const ServiceOptions& options,
    const CreateServiceCallback& callback,
    const CreateServiceErrorCallback& error_callback) {
  scoped_refptr<BluetoothSocketMac> socket = BluetoothSocketMac::CreateSocket();
  socket->ListenUsingL2cap(
      this, uuid, options, base::Bind(callback, socket), error_callback);
}

void BluetoothAdapterMac::RegisterAudioSink(
    const BluetoothAudioSink::Options& options,
    const AcquiredCallback& callback,
    const BluetoothAudioSink::ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
  error_callback.Run(BluetoothAudioSink::ERROR_UNSUPPORTED_PLATFORM);
}

void BluetoothAdapterMac::RegisterAdvertisement(
    std::unique_ptr<BluetoothAdvertisement::Data> advertisement_data,
    const CreateAdvertisementCallback& callback,
    const CreateAdvertisementErrorCallback& error_callback) {
  NOTIMPLEMENTED();
  error_callback.Run(BluetoothAdvertisement::ERROR_UNSUPPORTED_PLATFORM);
}

BluetoothLocalGattService* BluetoothAdapterMac::GetGattService(
    const std::string& identifier) const {
  return nullptr;
}

void BluetoothAdapterMac::ClassicDeviceFound(IOBluetoothDevice* device) {
  ClassicDeviceAdded(device);
}

void BluetoothAdapterMac::ClassicDiscoveryStopped(bool unexpected) {
  if (unexpected) {
    DVLOG(1) << "Discovery stopped unexpectedly";
    num_discovery_sessions_ = 0;
    MarkDiscoverySessionsAsInactive();
  }
  FOR_EACH_OBSERVER(BluetoothAdapter::Observer,
                    observers_,
                    AdapterDiscoveringChanged(this, false));
}

void BluetoothAdapterMac::DeviceConnected(IOBluetoothDevice* device) {
  // TODO(isherman): Investigate whether this method can be replaced with a call
  // to +registerForConnectNotifications:selector:.
  DVLOG(1) << "Adapter registered a new connection from device with address: "
           << BluetoothClassicDeviceMac::GetDeviceAddress(device);
  ClassicDeviceAdded(device);
}

// static
bool BluetoothAdapterMac::IsLowEnergyAvailable() {
  return base::mac::IsOSYosemiteOrLater();
}

void BluetoothAdapterMac::RemovePairingDelegateInternal(
    BluetoothDevice::PairingDelegate* pairing_delegate) {}

void BluetoothAdapterMac::SetCentralManagerForTesting(
    CBCentralManager* central_manager) {
  CHECK(BluetoothAdapterMac::IsLowEnergyAvailable());
  central_manager.delegate = low_energy_central_manager_delegate_;
  low_energy_central_manager_.reset(central_manager,
                                    base::scoped_policy::RETAIN);
  low_energy_discovery_manager_->SetCentralManager(
      low_energy_central_manager_.get());
}

CBCentralManager* BluetoothAdapterMac::GetCentralManager() {
  return low_energy_central_manager_;
}

void BluetoothAdapterMac::AddDiscoverySession(
    BluetoothDiscoveryFilter* discovery_filter,
    const base::Closure& callback,
    const DiscoverySessionErrorCallback& error_callback) {
  DVLOG(1) << __func__;
  if (num_discovery_sessions_ > 0) {
    DCHECK(IsDiscovering());
    num_discovery_sessions_++;
    // We are already running a discovery session, notify the system if the
    // filter has changed.
    if (!StartDiscovery(discovery_filter)) {
      // TODO: Provide a more precise error here.
      error_callback.Run(UMABluetoothDiscoverySessionOutcome::UNKNOWN);
      return;
    }
    callback.Run();
    return;
  }

  DCHECK_EQ(0, num_discovery_sessions_);

  if (!StartDiscovery(discovery_filter)) {
    // TODO: Provide a more precise error here.
    error_callback.Run(UMABluetoothDiscoverySessionOutcome::UNKNOWN);
    return;
  }

  DVLOG(1) << "Added a discovery session";
  num_discovery_sessions_++;
  FOR_EACH_OBSERVER(BluetoothAdapter::Observer,
                    observers_,
                    AdapterDiscoveringChanged(this, true));
  callback.Run();
}

void BluetoothAdapterMac::RemoveDiscoverySession(
    BluetoothDiscoveryFilter* discovery_filter,
    const base::Closure& callback,
    const DiscoverySessionErrorCallback& error_callback) {
  DVLOG(1) << __func__;

  if (num_discovery_sessions_ > 1) {
    // There are active sessions other than the one currently being removed.
    DCHECK(IsDiscovering());
    num_discovery_sessions_--;
    callback.Run();
    return;
  }

  if (num_discovery_sessions_ == 0) {
    DVLOG(1) << "No active discovery sessions. Returning error.";
    error_callback.Run(UMABluetoothDiscoverySessionOutcome::NOT_ACTIVE);
    return;
  }

  // Default to dual discovery if |discovery_filter| is NULL.
  BluetoothTransport transport = BLUETOOTH_TRANSPORT_DUAL;
  if (discovery_filter)
    transport = discovery_filter->GetTransport();

  if (transport & BLUETOOTH_TRANSPORT_CLASSIC) {
    if (!classic_discovery_manager_->StopDiscovery()) {
      DVLOG(1) << "Failed to stop classic discovery";
      // TODO: Provide a more precise error here.
      error_callback.Run(UMABluetoothDiscoverySessionOutcome::UNKNOWN);
      return;
    }
  }
  if (transport & BLUETOOTH_TRANSPORT_LE) {
    if (IsLowEnergyAvailable())
      low_energy_discovery_manager_->StopDiscovery();
  }

  DVLOG(1) << "Discovery stopped";
  num_discovery_sessions_--;
  callback.Run();
}

void BluetoothAdapterMac::SetDiscoveryFilter(
    std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
    const base::Closure& callback,
    const DiscoverySessionErrorCallback& error_callback) {
  NOTIMPLEMENTED();
  error_callback.Run(UMABluetoothDiscoverySessionOutcome::NOT_IMPLEMENTED);
}

bool BluetoothAdapterMac::StartDiscovery(
    BluetoothDiscoveryFilter* discovery_filter) {
  // Default to dual discovery if |discovery_filter| is NULL.  IOBluetooth seems
  // allow starting low energy and classic discovery at once.
  BluetoothTransport transport = BLUETOOTH_TRANSPORT_DUAL;
  if (discovery_filter)
    transport = discovery_filter->GetTransport();

  if ((transport & BLUETOOTH_TRANSPORT_CLASSIC) &&
      !classic_discovery_manager_->IsDiscovering()) {
    // TODO(krstnmnlsn): If a classic discovery session is already running then
    // we should update its filter. crbug.com/498056
    if (!classic_discovery_manager_->StartDiscovery()) {
      DVLOG(1) << "Failed to add a classic discovery session";
      return false;
    }
  }
  if (transport & BLUETOOTH_TRANSPORT_LE) {
    // Begin a low energy discovery session or update it if one is already
    // running.
    if (IsLowEnergyAvailable())
      low_energy_discovery_manager_->StartDiscovery(
          BluetoothDevice::UUIDList());
  }
  return true;
}

void BluetoothAdapterMac::Init() {
  ui_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  PollAdapter();
}

void BluetoothAdapterMac::InitForTest(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner) {
  ui_task_runner_ = ui_task_runner;
}

void BluetoothAdapterMac::PollAdapter() {
  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/461181
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile1(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "461181 BluetoothAdapterMac::PollAdapter::Start"));
  bool was_present = IsPresent();
  std::string address;
  bool classic_powered = false;
  IOBluetoothHostController* controller =
      [IOBluetoothHostController defaultController];

  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/461181
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile2(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "461181 BluetoothAdapterMac::PollAdapter::GetControllerStats"));
  if (controller != nil) {
    address = BluetoothDevice::CanonicalizeAddress(
        base::SysNSStringToUTF8([controller addressAsString]));
    classic_powered = ([controller powerState] == kBluetoothHCIPowerStateON);

    // For performance reasons, cache the adapter's name. It's not uncommon for
    // a call to [controller nameAsString] to take tens of milliseconds. Note
    // that this caching strategy might result in clients receiving a stale
    // name. If this is a significant issue, then some more sophisticated
    // workaround for the performance bottleneck will be needed. For additional
    // context, see http://crbug.com/461181 and http://crbug.com/467316
    if (address != address_ || (!address.empty() && name_.empty()))
      name_ = base::SysNSStringToUTF8([controller nameAsString]);
  }

  bool is_present = !address.empty();
  address_ = address;

  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/461181
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile3(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "461181 BluetoothAdapterMac::PollAdapter::AdapterPresentChanged"));
  if (was_present != is_present) {
    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      AdapterPresentChanged(this, is_present));
  }

  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/461181
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile4(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "461181 BluetoothAdapterMac::PollAdapter::AdapterPowerChanged"));
  if (classic_powered_ != classic_powered) {
    classic_powered_ = classic_powered;
    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      AdapterPoweredChanged(this, classic_powered_));
  }

  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/461181
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile5(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "461181 BluetoothAdapterMac::PollAdapter::RemoveTimedOutDevices"));
  RemoveTimedOutDevices();

  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/461181
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile6(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "461181 BluetoothAdapterMac::PollAdapter::AddPairedDevices"));
  AddPairedDevices();

  ui_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&BluetoothAdapterMac::PollAdapter,
                 weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kPollIntervalMs));
}

void BluetoothAdapterMac::ClassicDeviceAdded(IOBluetoothDevice* device) {
  std::string device_address =
      BluetoothClassicDeviceMac::GetDeviceAddress(device);

  // Only notify observers once per device.
  if (devices_.count(device_address))
    return;

  BluetoothDevice* device_classic = new BluetoothClassicDeviceMac(this, device);
  devices_.set(device_address, base::WrapUnique(device_classic));
  FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                    DeviceAdded(this, device_classic));
}

void BluetoothAdapterMac::LowEnergyDeviceUpdated(
    CBPeripheral* peripheral,
    NSDictionary* advertisement_data,
    int rssi) {
  BluetoothLowEnergyDeviceMac* device_mac =
      GetBluetoothLowEnergyDeviceMac(peripheral);
  // if has no entry in the map, create new device and insert into |devices_|,
  // otherwise update the existing device.
  if (!device_mac) {
    VLOG(1) << "LowEnergyDeviceUpdated new device";
    // A new device has been found.
    device_mac = new BluetoothLowEnergyDeviceMac(this, peripheral,
                                                 advertisement_data, rssi);
    std::string device_address =
        BluetoothLowEnergyDeviceMac::GetPeripheralHashAddress(peripheral);
    devices_.add(device_address, std::unique_ptr<BluetoothDevice>(device_mac));
    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      DeviceAdded(this, device_mac));
    return;
  }

  std::string stored_device_id = device_mac->GetIdentifier();
  std::string updated_device_id =
      BluetoothLowEnergyDeviceMac::GetPeripheralIdentifier(peripheral);
  if (stored_device_id != updated_device_id) {
    VLOG(1) << "LowEnergyDeviceUpdated stored_device_id != updated_device_id: "
            << std::endl
            << "  " << stored_device_id << std::endl
            << "  " << updated_device_id;
    // Collision, two identifiers map to the same hash address.  With a 48 bit
    // hash the probability of this occuring with 10,000 devices
    // simultaneously present is 1e-6 (see
    // https://en.wikipedia.org/wiki/Birthday_problem#Probability_table).  We
    // ignore the second device by returning.
    return;
  }

  // A device has an update.
  VLOG(2) << "LowEnergyDeviceUpdated";
  device_mac->Update(advertisement_data, rssi);
  // TODO(scheib): Call DeviceChanged only if UUIDs change. crbug.com/547106
  FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                    DeviceChanged(this, device_mac));
}

// TODO(krstnmnlsn): Implement. crbug.com/511025
void BluetoothAdapterMac::LowEnergyCentralManagerUpdatedState() {}

void BluetoothAdapterMac::AddPairedDevices() {
  // Add any new paired devices.
  for (IOBluetoothDevice* device in [IOBluetoothDevice pairedDevices]) {
    ClassicDeviceAdded(device);
  }
}

void BluetoothAdapterMac::CreateGattConnection(
    BluetoothLowEnergyDeviceMac* device_mac) {
  [low_energy_central_manager_ connectPeripheral:device_mac->peripheral_
                                         options:nil];
}

void BluetoothAdapterMac::DisconnectGatt(
    BluetoothLowEnergyDeviceMac* device_mac) {
  [low_energy_central_manager_
      cancelPeripheralConnection:device_mac->peripheral_];
}

void BluetoothAdapterMac::DidConnectPeripheral(CBPeripheral* peripheral) {
  BluetoothLowEnergyDeviceMac* device_mac =
      GetBluetoothLowEnergyDeviceMac(peripheral);
  if (!device_mac) {
    [low_energy_central_manager_ cancelPeripheralConnection:peripheral];
    return;
  }
  device_mac->DidConnectGatt();
  [device_mac->GetPeripheral() discoverServices:nil];
}

void BluetoothAdapterMac::DidFailToConnectPeripheral(CBPeripheral* peripheral,
                                                     NSError* error) {
  BluetoothLowEnergyDeviceMac* device_mac =
      GetBluetoothLowEnergyDeviceMac(peripheral);
  if (!device_mac) {
    [low_energy_central_manager_ cancelPeripheralConnection:peripheral];
    return;
  }
  VLOG(1) << "Bluetooth error, domain: " << error.domain.UTF8String
          << ", error code: " << error.code;
  BluetoothDevice::ConnectErrorCode error_code =
      BluetoothDeviceMac::GetConnectErrorCodeFromNSError(error);
  VLOG(1) << "Bluetooth error, domain: " << error.domain.UTF8String
          << ", error code: " << error.code
          << ", converted into: " << error_code;
  device_mac->DidFailToConnectGatt(error_code);
}

void BluetoothAdapterMac::DidDisconnectPeripheral(CBPeripheral* peripheral,
                                                  NSError* error) {
  BluetoothLowEnergyDeviceMac* device_mac =
      GetBluetoothLowEnergyDeviceMac(peripheral);
  if (!device_mac) {
    [low_energy_central_manager_ cancelPeripheralConnection:peripheral];
    return;
  }
  VLOG(1) << "Bluetooth error, domain: " << error.domain.UTF8String
          << ", error code: " << error.code;
  BluetoothDevice::ConnectErrorCode error_code =
      BluetoothDeviceMac::GetConnectErrorCodeFromNSError(error);
  device_mac->DidDisconnectPeripheral(error_code);
}

BluetoothLowEnergyDeviceMac*
BluetoothAdapterMac::GetBluetoothLowEnergyDeviceMac(CBPeripheral* peripheral) {
  std::string device_address =
      BluetoothLowEnergyDeviceMac::GetPeripheralHashAddress(peripheral);
  DevicesMap::const_iterator iter = devices_.find(device_address);
  if (iter == devices_.end()) {
    return nil;
  }
  return static_cast<BluetoothLowEnergyDeviceMac*>(iter->second);
}

}  // namespace device
