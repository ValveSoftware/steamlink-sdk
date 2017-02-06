// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DIAL_DIAL_REGISTRY_H_
#define CHROME_BROWSER_EXTENSIONS_API_DIAL_DIAL_REGISTRY_H_

#include <stddef.h>

#include <map>
#include <string>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/extensions/api/dial/dial_service.h"
#include "chrome/browser/profiles/profile.h"
#include "net/base/network_change_notifier.h"

namespace extensions {

// Keeps track of devices that have responded to discovery requests and notifies
// the observer with an updated, complete set of active devices.  The registry's
// observer (i.e., the Dial API) owns the registry instance.
class DialRegistry : public DialService::Observer,
                     public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  typedef std::vector<DialDeviceData> DeviceList;

  enum DialErrorCode {
    DIAL_NO_LISTENERS = 0,
    DIAL_NO_INTERFACES,
    DIAL_NETWORK_DISCONNECTED,
    DIAL_CELLULAR_NETWORK,
    DIAL_SOCKET_ERROR,
    DIAL_UNKNOWN
  };

  class Observer {
   public:
    // Methods invoked on the IO thread when a new device is discovered, an
    // update is triggered by dial.discoverNow or an error occured.
    virtual void OnDialDeviceEvent(const DeviceList& devices) = 0;
    virtual void OnDialError(DialErrorCode type) = 0;

   protected:
    virtual ~Observer() {}
  };

  // Create the DIAL registry and pass a reference to allow it to notify on
  // DIAL device events.
  DialRegistry(Observer* dial_api,
               const base::TimeDelta& refresh_interval,
               const base::TimeDelta& expiration,
               const size_t max_devices);

  ~DialRegistry() override;

  // Called by the DIAL API when event listeners are added or removed. The dial
  // service is started after the first listener is added and stopped after the
  // last listener is removed.
  void OnListenerAdded();
  void OnListenerRemoved();

  // Called by the DIAL API to try to kickoff a discovery if there is not one
  // already active.
  bool DiscoverNow();

 protected:
  // Returns a new instance of the DIAL service.  Overridden by tests.
  virtual DialService* CreateDialService();
  virtual void ClearDialService();

  // Returns the current time.  Overridden by tests.
  virtual base::Time Now() const;

 protected:
  // The DIAL service. Periodic discovery is active when this is not NULL.
  std::unique_ptr<DialService> dial_;

 private:
  typedef base::hash_map<std::string, linked_ptr<DialDeviceData> >
      DeviceByIdMap;
  typedef std::map<std::string, linked_ptr<DialDeviceData> > DeviceByLabelMap;

  // DialService::Observer:
  void OnDiscoveryRequest(DialService* service) override;
  void OnDeviceDiscovered(DialService* service,
                          const DialDeviceData& device) override;
  void OnDiscoveryFinished(DialService* service) override;
  void OnError(DialService* service,
               const DialService::DialServiceErrorCode& code) override;

  // net::NetworkChangeObserver:
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  // Starts and stops periodic discovery.  Periodic discovery is done when there
  // are registered event listeners.
  void StartPeriodicDiscovery();
  void StopPeriodicDiscovery();

  // Check whether we are in a state ready to discover and dispatch error
  // notifications if not.
  bool ReadyToDiscover();

  // Purge our whole registry. We may need to do this occasionally, e.g. when
  // the network status changes.  Increments the registry generation.
  void Clear();

  // The repeating timer schedules discoveries with this method.
  void DoDiscovery();

  // Attempts to add a newly discovered device to the registry.  Returns true if
  // successful.
  bool MaybeAddDevice(const linked_ptr<DialDeviceData>& device_data);

  // Remove devices from the registry that have expired, i.e. not responded
  // after some time.  Returns true if the registry was modified.
  bool PruneExpiredDevices();

  // Returns true if the device has expired and should be removed from the
  // active set.
  bool IsDeviceExpired(const DialDeviceData& device) const;

  // Notify listeners with the current device list if the list has changed.
  void MaybeSendEvent();

  // Notify listeners with the current device list.
  void SendEvent();

  // Returns the next label to use for a newly-seen device.
  std::string NextLabel();

  // The current number of event listeners attached to this registry.
  int num_listeners_;

  // Incremented each time we modify the registry of active devices.
  int registry_generation_;

  // The registry generation associated with the last time we sent an event.
  // Used to suppress events with duplicate device lists.
  int last_event_registry_generation_;

  // Counter to generate device labels.
  int label_count_;

  // Registry parameters
  base::TimeDelta refresh_interval_delta_;
  base::TimeDelta expiration_delta_;
  size_t max_devices_;

  // A map used to track known devices by their device_id.
  DeviceByIdMap device_by_id_map_;

  // A map used to track known devices sorted by label.  We iterate over this to
  // construct the device list sent to API clients.
  DeviceByLabelMap device_by_label_map_;

  // Timer used to manage periodic discovery requests.
  base::RepeatingTimer repeating_timer_;

  // Interface from which the DIAL API is notified of DIAL device events. the
  // DIAL API owns this DIAL registry.
  Observer* const dial_api_;

  // Thread checker.
  base::ThreadChecker thread_checker_;

  FRIEND_TEST_ALL_PREFIXES(DialRegistryTest, TestAddRemoveListeners);
  FRIEND_TEST_ALL_PREFIXES(DialRegistryTest, TestNoDevicesDiscovered);
  FRIEND_TEST_ALL_PREFIXES(DialRegistryTest, TestDevicesDiscovered);
  FRIEND_TEST_ALL_PREFIXES(DialRegistryTest,
                           TestDevicesDiscoveredWithTwoListeners);
  FRIEND_TEST_ALL_PREFIXES(DialRegistryTest, TestDeviceExpires);
  FRIEND_TEST_ALL_PREFIXES(DialRegistryTest, TestExpiredDeviceIsRediscovered);
  FRIEND_TEST_ALL_PREFIXES(DialRegistryTest,
                           TestRemovingListenerDoesNotClearList);
  FRIEND_TEST_ALL_PREFIXES(DialRegistryTest, TestNetworkEventConnectionLost);
  FRIEND_TEST_ALL_PREFIXES(DialRegistryTest,
                           TestNetworkEventConnectionRestored);
  DISALLOW_COPY_AND_ASSIGN(DialRegistry);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DIAL_DIAL_REGISTRY_H_
