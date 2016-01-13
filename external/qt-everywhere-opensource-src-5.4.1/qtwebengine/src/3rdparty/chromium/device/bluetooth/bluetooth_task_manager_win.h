// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_TASK_MANAGER_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_TASK_MANAGER_WIN_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "base/win/scoped_handle.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace base {

class SequencedTaskRunner;
class SequencedWorkerPool;

}  // namespace base

namespace device {

// Manages the blocking Bluetooth tasks using |SequencedWorkerPool|. It runs
// bluetooth tasks using |SequencedWorkerPool| and informs its observers of
// bluetooth adapter state changes and any other bluetooth device inquiry
// result.
//
// It delegates the blocking Windows API calls to |bluetooth_task_runner_|'s
// message loop, and receives responses via methods like OnAdapterStateChanged
// posted to UI thread.
class BluetoothTaskManagerWin
    : public base::RefCountedThreadSafe<BluetoothTaskManagerWin> {
 public:
  struct AdapterState {
    AdapterState();
    ~AdapterState();
    std::string name;
    std::string address;
    bool powered;
  };

  struct ServiceRecordState {
    ServiceRecordState();
    ~ServiceRecordState();
    std::string name;
    std::string address;
    std::vector<uint8> sdp_bytes;
  };

  struct DeviceState {
    DeviceState();
    ~DeviceState();
    std::string name;
    std::string address;
    uint32 bluetooth_class;
    bool visible;
    bool connected;
    bool authenticated;
    ScopedVector<ServiceRecordState> service_record_states;
  };

  class Observer {
   public:
     virtual ~Observer() {}

     virtual void AdapterStateChanged(const AdapterState& state) {}
     virtual void DiscoveryStarted(bool success) {}
     virtual void DiscoveryStopped() {}
     virtual void DevicesUpdated(const ScopedVector<DeviceState>& devices) {}
     virtual void DevicesDiscovered(const ScopedVector<DeviceState>& devices) {}
  };

  explicit BluetoothTaskManagerWin(
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void Initialize();
  void InitializeWithBluetoothTaskRunner(
      scoped_refptr<base::SequencedTaskRunner> bluetooth_task_runner);
  void Shutdown();

  void PostSetPoweredBluetoothTask(
      bool powered,
      const base::Closure& callback,
      const BluetoothAdapter::ErrorCallback& error_callback);
  void PostStartDiscoveryTask();
  void PostStopDiscoveryTask();

 private:
  friend class base::RefCountedThreadSafe<BluetoothTaskManagerWin>;
  friend class BluetoothTaskManagerWinTest;

  static const int kPollIntervalMs;

  virtual ~BluetoothTaskManagerWin();

  // Notify all Observers of updated AdapterState. Should only be called on the
  // UI thread.
  void OnAdapterStateChanged(const AdapterState* state);
  void OnDiscoveryStarted(bool success);
  void OnDiscoveryStopped();
  void OnDevicesUpdated(const ScopedVector<DeviceState>* devices);
  void OnDevicesDiscovered(const ScopedVector<DeviceState>* devices);

  // Called on BluetoothTaskRunner.
  void StartPolling();
  void PollAdapter();
  void PostAdapterStateToUi();
  void SetPowered(bool powered,
                  const base::Closure& callback,
                  const BluetoothAdapter::ErrorCallback& error_callback);

  // Starts discovery. Once the discovery starts, it issues a discovery inquiry
  // with a short timeout, then issues more inquiries with greater timeout
  // values. The discovery finishes when StopDiscovery() is called or timeout
  // has reached its maximum value.
  void StartDiscovery();
  void StopDiscovery();

  // Issues a device inquiry that runs for |timeout| * 1.28 seconds.
  // This posts itself again with |timeout| + 1 until |timeout| reaches the
  // maximum value or stop discovery call is received.
  void DiscoverDevices(int timeout);

  // Fetch already known device information. Similar to |StartDiscovery|, except
  // this function does not issue a discovery inquiry. Instead it gets the
  // device info cached in the adapter.
  void GetKnownDevices();

  // Sends a device search API call to the adapter.
  void SearchDevices(int timeout,
                     bool search_cached_devices_only,
                     ScopedVector<DeviceState>* device_list);

  // Discover services for the devices in |device_list|.
  void DiscoverServices(ScopedVector<DeviceState>* device_list);

  // UI task runner reference.
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  scoped_refptr<base::SequencedWorkerPool> worker_pool_;
  scoped_refptr<base::SequencedTaskRunner> bluetooth_task_runner_;

  // List of observers interested in event notifications.
  ObserverList<Observer> observers_;

  // Adapter handle owned by bluetooth task runner.
  base::win::ScopedHandle adapter_handle_;

  // indicates whether the adapter is in discovery mode or not.
  bool discovering_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothTaskManagerWin);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_TASK_MANAGER_WIN_H_
