// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_ADAPTER_FACTORY_WRAPPER_H_
#define CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_ADAPTER_FACTORY_WRAPPER_H_

#include <unordered_set>

#include "base/macros.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace content {

class WebBluetoothServiceImpl;

// Wrapper around BluetoothAdapterFactory that allows us to change
// the underlying BluetoothAdapter object and have the observers
// observe the new instance of the object.
// TODO(ortuno): Once there is no need to swap the adapter to change its
// behavior observers should add/remove themselves to/from the adapter.
// http://crbug.com/603291
class CONTENT_EXPORT BluetoothAdapterFactoryWrapper final {
 public:
  typedef base::Callback<void(device::BluetoothAdapter*)>
      AcquireAdapterCallback;

  BluetoothAdapterFactoryWrapper();
  ~BluetoothAdapterFactoryWrapper();

  // Returns true if the platform supports Bluetooth or if
  // SetBluetoothAdapterForTesting has been called.
  bool IsBluetoothAdapterAvailable();

  // Adds |observer| to the set of adapter observers. If another observer has
  // acquired the adapter in the past it adds |observer| as an observer to that
  // adapter, otherwise it gets a new adapter and adds |observer| to it. Runs
  // |callback| with the adapter |observer| has been added to.
  void AcquireAdapter(device::BluetoothAdapter::Observer* observer,
                      const AcquireAdapterCallback& callback);
  // Removes |observer| from the list of adapter observers if |observer|
  // has acquired the adapter in the past. If there are no more observers
  // it deletes the reference to the adapter.
  void ReleaseAdapter(device::BluetoothAdapter::Observer* observer);

  // Returns an adapter if |observer| has acquired an adapter in the past and
  // this instance holds a reference to an adapter. Otherwise returns nullptr.
  device::BluetoothAdapter* GetAdapter(
      device::BluetoothAdapter::Observer* observer);

  // The period of time a device discovery session should be active for.
  // Returns 0 if SetBluetoothAdapterForTesting has been called.
  base::TimeDelta GetScanDuration() { return scan_duration_; }

  // Sets a new BluetoothAdapter to be returned by GetAdapter. When setting
  // a new adapter all observers from the old adapter are removed and added
  // to |mock_adapter|.
  void SetBluetoothAdapterForTesting(
      scoped_refptr<device::BluetoothAdapter> mock_adapter);

 private:
  void OnGetAdapter(const AcquireAdapterCallback& continuation,
                    scoped_refptr<device::BluetoothAdapter> adapter);

  bool HasAdapter(device::BluetoothAdapter::Observer* observer);
  void AddAdapterObserver(device::BluetoothAdapter::Observer* observer);
  void RemoveAdapterObserver(device::BluetoothAdapter::Observer* observer);

  // Sets |adapter_| to a BluetoothAdapter instance and register observers,
  // releasing references to previous |adapter_|.
  void set_adapter(scoped_refptr<device::BluetoothAdapter> adapter);

  // A BluetoothAdapter instance representing an adapter of the system.
  scoped_refptr<device::BluetoothAdapter> adapter_;

  // We keep a list of all observers so that when the adapter gets swapped,
  // we can remove all observers from the old adapter and add them to the
  // new adapter.
  std::unordered_set<device::BluetoothAdapter::Observer*> adapter_observers_;

  // This is 0 if SetBluetoothAdapterForTesting has been called.
  base::TimeDelta scan_duration_;

  bool testing_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothAdapterFactoryWrapper> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothAdapterFactoryWrapper);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_ADAPTER_FACTORY_WRAPPER_H_
