// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_adapter_win.h"

#include <hash_set>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/thread_task_runner_handle.h"
#include "device/bluetooth/bluetooth_device_win.h"
#include "device/bluetooth/bluetooth_socket_thread.h"
#include "device/bluetooth/bluetooth_socket_win.h"
#include "device/bluetooth/bluetooth_task_manager_win.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace device {

// static
base::WeakPtr<BluetoothAdapter> BluetoothAdapter::CreateAdapter(
    const InitCallback& init_callback) {
  return BluetoothAdapterWin::CreateAdapter(init_callback);
}

// static
base::WeakPtr<BluetoothAdapter> BluetoothAdapterWin::CreateAdapter(
    const InitCallback& init_callback) {
  BluetoothAdapterWin* adapter = new BluetoothAdapterWin(init_callback);
  adapter->Init();
  return adapter->weak_ptr_factory_.GetWeakPtr();
}

BluetoothAdapterWin::BluetoothAdapterWin(const InitCallback& init_callback)
    : BluetoothAdapter(),
      init_callback_(init_callback),
      initialized_(false),
      powered_(false),
      discovery_status_(NOT_DISCOVERING),
      num_discovery_listeners_(0),
      weak_ptr_factory_(this) {
}

BluetoothAdapterWin::~BluetoothAdapterWin() {
  if (task_manager_) {
    task_manager_->RemoveObserver(this);
    task_manager_->Shutdown();
  }
}

void BluetoothAdapterWin::AddObserver(BluetoothAdapter::Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void BluetoothAdapterWin::RemoveObserver(BluetoothAdapter::Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

std::string BluetoothAdapterWin::GetAddress() const {
  return address_;
}

std::string BluetoothAdapterWin::GetName() const {
  return name_;
}

void BluetoothAdapterWin::SetName(const std::string& name,
                                  const base::Closure& callback,
                                  const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

// TODO(youngki): Return true when |task_manager_| initializes the adapter
// state.
bool BluetoothAdapterWin::IsInitialized() const {
  return initialized_;
}

bool BluetoothAdapterWin::IsPresent() const {
  return !address_.empty();
}

bool BluetoothAdapterWin::IsPowered() const {
  return powered_;
}

void BluetoothAdapterWin::SetPowered(
    bool powered,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  task_manager_->PostSetPoweredBluetoothTask(powered, callback, error_callback);
}

bool BluetoothAdapterWin::IsDiscoverable() const {
  NOTIMPLEMENTED();
  return false;
}

void BluetoothAdapterWin::SetDiscoverable(
    bool discoverable,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  NOTIMPLEMENTED();
}

bool BluetoothAdapterWin::IsDiscovering() const {
  return discovery_status_ == DISCOVERING ||
      discovery_status_ == DISCOVERY_STOPPING;
}

void BluetoothAdapterWin::DiscoveryStarted(bool success) {
  discovery_status_ = success ? DISCOVERING : NOT_DISCOVERING;
  for (std::vector<std::pair<base::Closure, ErrorCallback> >::const_iterator
       iter = on_start_discovery_callbacks_.begin();
       iter != on_start_discovery_callbacks_.end();
       ++iter) {
    if (success)
      ui_task_runner_->PostTask(FROM_HERE, iter->first);
    else
      ui_task_runner_->PostTask(FROM_HERE, iter->second);
  }
  num_discovery_listeners_ = on_start_discovery_callbacks_.size();
  on_start_discovery_callbacks_.clear();

  if (success) {
    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      AdapterDiscoveringChanged(this, true));

    // If there are stop discovery requests, post the stop discovery again.
    MaybePostStopDiscoveryTask();
  } else if (!on_stop_discovery_callbacks_.empty()) {
    // If there are stop discovery requests but start discovery has failed,
    // notify that stop discovery has been complete.
    DiscoveryStopped();
  }
}

void BluetoothAdapterWin::DiscoveryStopped() {
  discovered_devices_.clear();
  bool was_discovering = IsDiscovering();
  discovery_status_ = NOT_DISCOVERING;
  for (std::vector<base::Closure>::const_iterator iter =
           on_stop_discovery_callbacks_.begin();
       iter != on_stop_discovery_callbacks_.end();
       ++iter) {
    ui_task_runner_->PostTask(FROM_HERE, *iter);
  }
  num_discovery_listeners_ = 0;
  on_stop_discovery_callbacks_.clear();
  if (was_discovering)
    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      AdapterDiscoveringChanged(this, false));

  // If there are start discovery requests, post the start discovery again.
  MaybePostStartDiscoveryTask();
}

void BluetoothAdapterWin::CreateRfcommService(
    const BluetoothUUID& uuid,
    int channel,
    const CreateServiceCallback& callback,
    const CreateServiceErrorCallback& error_callback) {
  scoped_refptr<BluetoothSocketWin> socket =
      BluetoothSocketWin::CreateBluetoothSocket(
          ui_task_runner_,
          socket_thread_,
          NULL,
          net::NetLog::Source());
  socket->Listen(this, uuid, channel,
                 base::Bind(callback, socket),
                 error_callback);
}

void BluetoothAdapterWin::CreateL2capService(
    const BluetoothUUID& uuid,
    int psm,
    const CreateServiceCallback& callback,
    const CreateServiceErrorCallback& error_callback) {
  // TODO(keybuk): implement.
  NOTIMPLEMENTED();
}

void BluetoothAdapterWin::RemovePairingDelegateInternal(
    BluetoothDevice::PairingDelegate* pairing_delegate) {
}

void BluetoothAdapterWin::AdapterStateChanged(
    const BluetoothTaskManagerWin::AdapterState& state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  name_ = state.name;
  bool was_present = IsPresent();
  bool is_present = !state.address.empty();
  address_ = BluetoothDevice::CanonicalizeAddress(state.address);
  if (was_present != is_present) {
    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      AdapterPresentChanged(this, is_present));
  }
  if (powered_ != state.powered) {
    powered_ = state.powered;
    FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                      AdapterPoweredChanged(this, powered_));
  }
  if (!initialized_) {
    initialized_ = true;
    init_callback_.Run();
  }
}

void BluetoothAdapterWin::DevicesDiscovered(
    const ScopedVector<BluetoothTaskManagerWin::DeviceState>& devices) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (ScopedVector<BluetoothTaskManagerWin::DeviceState>::const_iterator iter =
           devices.begin();
       iter != devices.end();
       ++iter) {
    if (discovered_devices_.find((*iter)->address) ==
        discovered_devices_.end()) {
      BluetoothDeviceWin device_win(
          **iter, ui_task_runner_, socket_thread_, NULL, net::NetLog::Source());
      FOR_EACH_OBSERVER(BluetoothAdapter::Observer, observers_,
                        DeviceAdded(this, &device_win));
      discovered_devices_.insert((*iter)->address);
    }
  }
}

void BluetoothAdapterWin::DevicesUpdated(
    const ScopedVector<BluetoothTaskManagerWin::DeviceState>& devices) {
  STLDeleteValues(&devices_);
  for (ScopedVector<BluetoothTaskManagerWin::DeviceState>::const_iterator iter =
           devices.begin();
       iter != devices.end();
       ++iter) {
    devices_[(*iter)->address] = new BluetoothDeviceWin(
        **iter, ui_task_runner_, socket_thread_, NULL, net::NetLog::Source());
  }
}

// If the method is called when |discovery_status_| is DISCOVERY_STOPPING,
// starting again is handled by BluetoothAdapterWin::DiscoveryStopped().
void BluetoothAdapterWin::AddDiscoverySession(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  if (discovery_status_ == DISCOVERING) {
    num_discovery_listeners_++;
    callback.Run();
    return;
  }
  on_start_discovery_callbacks_.push_back(
      std::make_pair(callback, error_callback));
  MaybePostStartDiscoveryTask();
}

void BluetoothAdapterWin::RemoveDiscoverySession(
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  if (discovery_status_ == NOT_DISCOVERING) {
    error_callback.Run();
    return;
  }
  on_stop_discovery_callbacks_.push_back(callback);
  MaybePostStopDiscoveryTask();
}

void BluetoothAdapterWin::Init() {
  ui_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  socket_thread_ = BluetoothSocketThread::Get();
  task_manager_ =
      new BluetoothTaskManagerWin(ui_task_runner_);
  task_manager_->AddObserver(this);
  task_manager_->Initialize();
}

void BluetoothAdapterWin::InitForTest(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<base::SequencedTaskRunner> bluetooth_task_runner) {
  ui_task_runner_ = ui_task_runner;
  task_manager_ =
      new BluetoothTaskManagerWin(ui_task_runner_);
  task_manager_->AddObserver(this);
  task_manager_->InitializeWithBluetoothTaskRunner(bluetooth_task_runner);
}

void BluetoothAdapterWin::MaybePostStartDiscoveryTask() {
  if (discovery_status_ == NOT_DISCOVERING &&
      !on_start_discovery_callbacks_.empty()) {
    discovery_status_ = DISCOVERY_STARTING;
    task_manager_->PostStartDiscoveryTask();
  }
}

void BluetoothAdapterWin::MaybePostStopDiscoveryTask() {
  if (discovery_status_ != DISCOVERING)
    return;

  if (on_stop_discovery_callbacks_.size() < num_discovery_listeners_) {
    for (std::vector<base::Closure>::const_iterator iter =
             on_stop_discovery_callbacks_.begin();
         iter != on_stop_discovery_callbacks_.end();
         ++iter) {
      ui_task_runner_->PostTask(FROM_HERE, *iter);
    }
    num_discovery_listeners_ -= on_stop_discovery_callbacks_.size();
    on_stop_discovery_callbacks_.clear();
    return;
  }

  discovery_status_ = DISCOVERY_STOPPING;
  task_manager_->PostStopDiscoveryTask();
}

}  // namespace device
