// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_task_manager_win.h"

#include <winsock2.h>

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/win/scoped_handle.h"
#include "device/bluetooth/bluetooth_init_win.h"
#include "device/bluetooth/bluetooth_service_record_win.h"
#include "net/base/winsock_init.h"

namespace {

const int kNumThreadsInWorkerPool = 3;
const char kBluetoothThreadName[] = "BluetoothPollingThreadWin";
const int kMaxNumDeviceAddressChar = 127;
const int kServiceDiscoveryResultBufferSize = 5000;
const int kMaxDeviceDiscoveryTimeout = 48;

typedef device::BluetoothTaskManagerWin::ServiceRecordState ServiceRecordState;

// Populates bluetooth adapter state using adapter_handle.
void GetAdapterState(HANDLE adapter_handle,
                     device::BluetoothTaskManagerWin::AdapterState* state) {
  std::string name;
  std::string address;
  bool powered = false;
  BLUETOOTH_RADIO_INFO adapter_info = { sizeof(BLUETOOTH_RADIO_INFO), 0 };
  if (adapter_handle &&
      ERROR_SUCCESS == BluetoothGetRadioInfo(adapter_handle,
                                             &adapter_info)) {
    name = base::SysWideToUTF8(adapter_info.szName);
    address = base::StringPrintf("%02X:%02X:%02X:%02X:%02X:%02X",
        adapter_info.address.rgBytes[5],
        adapter_info.address.rgBytes[4],
        adapter_info.address.rgBytes[3],
        adapter_info.address.rgBytes[2],
        adapter_info.address.rgBytes[1],
        adapter_info.address.rgBytes[0]);
    powered = !!BluetoothIsConnectable(adapter_handle);
  }
  state->name = name;
  state->address = address;
  state->powered = powered;
}

void GetDeviceState(const BLUETOOTH_DEVICE_INFO& device_info,
                    device::BluetoothTaskManagerWin::DeviceState* state) {
  state->name = base::SysWideToUTF8(device_info.szName);
  state->address = base::StringPrintf("%02X:%02X:%02X:%02X:%02X:%02X",
      device_info.Address.rgBytes[5],
      device_info.Address.rgBytes[4],
      device_info.Address.rgBytes[3],
      device_info.Address.rgBytes[2],
      device_info.Address.rgBytes[1],
      device_info.Address.rgBytes[0]);
  state->bluetooth_class = device_info.ulClassofDevice;
  state->visible = true;
  state->connected = !!device_info.fConnected;
  state->authenticated = !!device_info.fAuthenticated;
}

void DiscoverDeviceServices(
    const std::string& device_address,
    const GUID& protocol_uuid,
    ScopedVector<ServiceRecordState>* service_record_states) {
  // Bluetooth and WSAQUERYSET for Service Inquiry. See http://goo.gl/2v9pyt.
  WSAQUERYSET sdp_query;
  ZeroMemory(&sdp_query, sizeof(sdp_query));
  sdp_query.dwSize = sizeof(sdp_query);
  GUID protocol = protocol_uuid;
  sdp_query.lpServiceClassId = &protocol;
  sdp_query.dwNameSpace = NS_BTH;
  wchar_t device_address_context[kMaxNumDeviceAddressChar];
  std::size_t length = base::SysUTF8ToWide("(" + device_address + ")").copy(
      device_address_context, kMaxNumDeviceAddressChar);
  device_address_context[length] = NULL;
  sdp_query.lpszContext = device_address_context;
  HANDLE sdp_handle;
  if (ERROR_SUCCESS !=
      WSALookupServiceBegin(&sdp_query, LUP_RETURN_ALL, &sdp_handle)) {
    return;
  }
  char sdp_buffer[kServiceDiscoveryResultBufferSize];
  LPWSAQUERYSET sdp_result_data = reinterpret_cast<LPWSAQUERYSET>(sdp_buffer);
  while (true) {
    DWORD sdp_buffer_size = sizeof(sdp_buffer);
    if (ERROR_SUCCESS !=
        WSALookupServiceNext(
            sdp_handle, LUP_RETURN_ALL, &sdp_buffer_size, sdp_result_data)) {
      break;
    }
    ServiceRecordState* service_record_state = new ServiceRecordState();
    service_record_state->name =
        base::SysWideToUTF8(sdp_result_data->lpszServiceInstanceName);
    service_record_state->address = device_address;
    for (uint64 i = 0; i < sdp_result_data->lpBlob->cbSize; i++) {
      service_record_state->sdp_bytes.push_back(
          sdp_result_data->lpBlob->pBlobData[i]);
    }
    service_record_states->push_back(service_record_state);
  }
  WSALookupServiceEnd(sdp_handle);
}

}  // namespace

namespace device {

// static
const int BluetoothTaskManagerWin::kPollIntervalMs = 500;

BluetoothTaskManagerWin::AdapterState::AdapterState() : powered(false) {
}

BluetoothTaskManagerWin::AdapterState::~AdapterState() {
}

BluetoothTaskManagerWin::ServiceRecordState::ServiceRecordState() {
}

BluetoothTaskManagerWin::ServiceRecordState::~ServiceRecordState() {
}

BluetoothTaskManagerWin::DeviceState::DeviceState()
    : bluetooth_class(0),
      visible(false),
      connected(false),
      authenticated(false) {
}

BluetoothTaskManagerWin::DeviceState::~DeviceState() {
}

BluetoothTaskManagerWin::BluetoothTaskManagerWin(
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner)
    : ui_task_runner_(ui_task_runner),
      discovering_(false) {
}

BluetoothTaskManagerWin::~BluetoothTaskManagerWin() {
}

void BluetoothTaskManagerWin::AddObserver(Observer* observer) {
  DCHECK(observer);
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  observers_.AddObserver(observer);
}

void BluetoothTaskManagerWin::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  observers_.RemoveObserver(observer);
}

void BluetoothTaskManagerWin::Initialize() {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  worker_pool_ = new base::SequencedWorkerPool(kNumThreadsInWorkerPool,
                                               kBluetoothThreadName);
  InitializeWithBluetoothTaskRunner(
      worker_pool_->GetSequencedTaskRunnerWithShutdownBehavior(
          worker_pool_->GetSequenceToken(),
          base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN));
}

void BluetoothTaskManagerWin::InitializeWithBluetoothTaskRunner(
    scoped_refptr<base::SequencedTaskRunner> bluetooth_task_runner) {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  bluetooth_task_runner_ = bluetooth_task_runner;
  bluetooth_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&BluetoothTaskManagerWin::StartPolling, this));
}

void BluetoothTaskManagerWin::StartPolling() {
  DCHECK(bluetooth_task_runner_->RunsTasksOnCurrentThread());

  if (device::bluetooth_init_win::HasBluetoothStack()) {
    PollAdapter();
  } else {
    // IF the bluetooth stack is not available, we still send an empty state
    // to BluetoothAdapter so that it is marked initialized, but the adapter
    // will not be present.
    AdapterState* state = new AdapterState();
    ui_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&BluetoothTaskManagerWin::OnAdapterStateChanged,
                 this,
                 base::Owned(state)));
  }
}

void BluetoothTaskManagerWin::Shutdown() {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  if (worker_pool_)
    worker_pool_->Shutdown();
}

void BluetoothTaskManagerWin::PostSetPoweredBluetoothTask(
    bool powered,
    const base::Closure& callback,
    const BluetoothAdapter::ErrorCallback& error_callback) {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  bluetooth_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&BluetoothTaskManagerWin::SetPowered,
                 this,
                 powered,
                 callback,
                 error_callback));
}

void BluetoothTaskManagerWin::PostStartDiscoveryTask() {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  bluetooth_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&BluetoothTaskManagerWin::StartDiscovery, this));
}

void BluetoothTaskManagerWin::PostStopDiscoveryTask() {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  bluetooth_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&BluetoothTaskManagerWin::StopDiscovery, this));
}

void BluetoothTaskManagerWin::OnAdapterStateChanged(const AdapterState* state) {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  FOR_EACH_OBSERVER(BluetoothTaskManagerWin::Observer, observers_,
                    AdapterStateChanged(*state));
}

void BluetoothTaskManagerWin::OnDiscoveryStarted(bool success) {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  FOR_EACH_OBSERVER(BluetoothTaskManagerWin::Observer, observers_,
                    DiscoveryStarted(success));
}

void BluetoothTaskManagerWin::OnDiscoveryStopped() {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  FOR_EACH_OBSERVER(BluetoothTaskManagerWin::Observer, observers_,
                    DiscoveryStopped());
}

void BluetoothTaskManagerWin::OnDevicesUpdated(
    const ScopedVector<DeviceState>* devices) {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  FOR_EACH_OBSERVER(BluetoothTaskManagerWin::Observer, observers_,
                    DevicesUpdated(*devices));
}

void BluetoothTaskManagerWin::OnDevicesDiscovered(
    const ScopedVector<DeviceState>* devices) {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  FOR_EACH_OBSERVER(BluetoothTaskManagerWin::Observer, observers_,
                    DevicesDiscovered(*devices));
}

void BluetoothTaskManagerWin::PollAdapter() {
  DCHECK(bluetooth_task_runner_->RunsTasksOnCurrentThread());

  // Skips updating the adapter info if the adapter is in discovery mode.
  if (!discovering_) {
    const BLUETOOTH_FIND_RADIO_PARAMS adapter_param =
        { sizeof(BLUETOOTH_FIND_RADIO_PARAMS) };
    if (adapter_handle_)
      adapter_handle_.Close();
    HANDLE temp_adapter_handle;
    HBLUETOOTH_RADIO_FIND handle = BluetoothFindFirstRadio(
        &adapter_param, &temp_adapter_handle);

    if (handle) {
      adapter_handle_.Set(temp_adapter_handle);
      GetKnownDevices();
      BluetoothFindRadioClose(handle);
    }
    PostAdapterStateToUi();
  }

  // Re-poll.
  bluetooth_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&BluetoothTaskManagerWin::PollAdapter,
                 this),
      base::TimeDelta::FromMilliseconds(kPollIntervalMs));
}

void BluetoothTaskManagerWin::PostAdapterStateToUi() {
  DCHECK(bluetooth_task_runner_->RunsTasksOnCurrentThread());
  AdapterState* state = new AdapterState();
  GetAdapterState(adapter_handle_, state);
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&BluetoothTaskManagerWin::OnAdapterStateChanged,
                 this,
                 base::Owned(state)));
}

void BluetoothTaskManagerWin::SetPowered(
    bool powered,
    const base::Closure& callback,
    const BluetoothAdapter::ErrorCallback& error_callback) {
  DCHECK(bluetooth_task_runner_->RunsTasksOnCurrentThread());
  bool success = false;
  if (adapter_handle_) {
    if (!powered)
      BluetoothEnableDiscovery(adapter_handle_, false);
    success = !!BluetoothEnableIncomingConnections(adapter_handle_, powered);
  }

  if (success) {
    PostAdapterStateToUi();
    ui_task_runner_->PostTask(FROM_HERE, callback);
  } else {
    ui_task_runner_->PostTask(FROM_HERE, error_callback);
  }
}

void BluetoothTaskManagerWin::StartDiscovery() {
  DCHECK(bluetooth_task_runner_->RunsTasksOnCurrentThread());
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&BluetoothTaskManagerWin::OnDiscoveryStarted,
                 this,
                 !!adapter_handle_));
  if (!adapter_handle_)
    return;
  discovering_ = true;

  DiscoverDevices(1);
}

void BluetoothTaskManagerWin::StopDiscovery() {
  DCHECK(bluetooth_task_runner_->RunsTasksOnCurrentThread());
  discovering_ = false;
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&BluetoothTaskManagerWin::OnDiscoveryStopped, this));
}

void BluetoothTaskManagerWin::DiscoverDevices(int timeout) {
  DCHECK(bluetooth_task_runner_->RunsTasksOnCurrentThread());
  if (!discovering_ || !adapter_handle_) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&BluetoothTaskManagerWin::OnDiscoveryStopped, this));
    return;
  }

  ScopedVector<DeviceState>* device_list = new ScopedVector<DeviceState>();
  SearchDevices(timeout, false, device_list);
  if (device_list->empty()) {
    delete device_list;
  } else {
    DiscoverServices(device_list);
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&BluetoothTaskManagerWin::OnDevicesDiscovered,
                   this,
                   base::Owned(device_list)));
  }

  if (timeout < kMaxDeviceDiscoveryTimeout) {
    bluetooth_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&BluetoothTaskManagerWin::DiscoverDevices,
                   this,
                   timeout + 1));
  } else {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&BluetoothTaskManagerWin::OnDiscoveryStopped, this));
    discovering_ = false;
  }
}

void BluetoothTaskManagerWin::GetKnownDevices() {
  ScopedVector<DeviceState>* device_list = new ScopedVector<DeviceState>();
  SearchDevices(1, true, device_list);
  if (device_list->empty()) {
    delete device_list;
    return;
  }
  DiscoverServices(device_list);
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&BluetoothTaskManagerWin::OnDevicesUpdated,
                 this,
                 base::Owned(device_list)));
}

void BluetoothTaskManagerWin::SearchDevices(
    int timeout,
    bool search_cached_devices_only,
    ScopedVector<DeviceState>* device_list) {
  BLUETOOTH_DEVICE_SEARCH_PARAMS device_search_params = {
      sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS),
      1,  // return authenticated devices
      1,  // return remembered devicess
      search_cached_devices_only ? 0 : 1,  // return unknown devices
      1,  // return connected devices
      search_cached_devices_only ? 0 : 1,  // issue a new inquiry
      timeout,  // timeout for the inquiry in increments of 1.28 seconds
      adapter_handle_
  };

  BLUETOOTH_DEVICE_INFO device_info = { sizeof(BLUETOOTH_DEVICE_INFO), 0 };
  // Issues a device inquiry and waits for |timeout| * 1.28 seconds.
  HBLUETOOTH_DEVICE_FIND handle =
      BluetoothFindFirstDevice(&device_search_params, &device_info);
  if (handle) {
    do {
      DeviceState* device_state = new DeviceState();
      GetDeviceState(device_info, device_state);
      device_list->push_back(device_state);
    } while (BluetoothFindNextDevice(handle, &device_info));

    BluetoothFindDeviceClose(handle);
  }
}

void BluetoothTaskManagerWin::DiscoverServices(
    ScopedVector<DeviceState>* device_list) {
  DCHECK(bluetooth_task_runner_->RunsTasksOnCurrentThread());
  net::EnsureWinsockInit();
  for (ScopedVector<DeviceState>::iterator iter = device_list->begin();
      iter != device_list->end();
      ++iter) {
    const std::string device_address = (*iter)->address;
    ScopedVector<ServiceRecordState>* service_record_states =
        &(*iter)->service_record_states;

    DiscoverDeviceServices(
        device_address, L2CAP_PROTOCOL_UUID, service_record_states);
  }
}

}  // namespace device
