// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_ADDRESS_TRACKER_LINUX_H_
#define NET_BASE_ADDRESS_TRACKER_LINUX_H_

#include <sys/socket.h>  // Needed to include netlink.
// Mask superfluous definition of |struct net|. This is fixed in Linux 2.6.38.
#define net net_kernel
#include <linux/rtnetlink.h>
#undef net

#include <map>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "net/base/net_util.h"
#include "net/base/network_change_notifier.h"

namespace net {
namespace internal {

// Keeps track of network interface addresses using rtnetlink. Used by
// NetworkChangeNotifier to provide signals to registered IPAddressObservers.
class NET_EXPORT_PRIVATE AddressTrackerLinux :
    public base::MessageLoopForIO::Watcher {
 public:
  typedef std::map<IPAddressNumber, struct ifaddrmsg> AddressMap;

  // Will run |address_callback| when the AddressMap changes, |link_callback|
  // when the list of online links changes, and |tunnel_callback| when the list
  // of online tunnels changes.
  AddressTrackerLinux(const base::Closure& address_callback,
                      const base::Closure& link_callback,
                      const base::Closure& tunnel_callback);
  virtual ~AddressTrackerLinux();

  // Starts watching system configuration for changes. The current thread must
  // have a MessageLoopForIO.
  void Init();

  AddressMap GetAddressMap() const;

  // Implementation of NetworkChangeNotifierLinux::GetCurrentConnectionType().
  // Safe to call from any thread, but will block until Init() has completed.
  NetworkChangeNotifier::ConnectionType GetCurrentConnectionType();

 private:
  friend class AddressTrackerLinuxTest;

  // A function that returns the name of an interface given the interface index
  // in |interface_index|.
  typedef const char* (*GetInterfaceNameFunction)(int interface_index);

  // Sets |*address_changed| to indicate whether |address_map_| changed and
  // sets |*link_changed| to indicate if |online_links_| changed and sets
  // |*tunnel_changed| to indicate if |online_links_| changed with regards to a
  // tunnel interface while reading messages from |netlink_fd_|.
  void ReadMessages(bool* address_changed,
                    bool* link_changed,
                    bool* tunnel_changed);

  // Sets |*address_changed| to true if |address_map_| changed, sets
  // |*link_changed| to true if |online_links_| changed, sets |*tunnel_changed|
  // to true if |online_links_| changed with regards to a tunnel interface while
  // reading the message from |buffer|.
  void HandleMessage(char* buffer,
                     size_t length,
                     bool* address_changed,
                     bool* link_changed,
                     bool* tunnel_changed);

  // Call when some part of initialization failed; forces online and unblocks.
  void AbortAndForceOnline();

  // MessageLoopForIO::Watcher:
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE;
  virtual void OnFileCanWriteWithoutBlocking(int /* fd */) OVERRIDE;

  // Close |netlink_fd_|
  void CloseSocket();

  // Does |msg| refer to a tunnel interface?
  bool IsTunnelInterface(const struct ifinfomsg* msg) const;

  // Gets the name of an interface given the interface index |interface_index|.
  // May return empty string if it fails but should not return NULL. This is
  // overridden by tests.
  GetInterfaceNameFunction get_interface_name_;

  base::Closure address_callback_;
  base::Closure link_callback_;
  base::Closure tunnel_callback_;

  int netlink_fd_;
  base::MessageLoopForIO::FileDescriptorWatcher watcher_;

  mutable base::Lock address_map_lock_;
  AddressMap address_map_;

  // Set of interface indices for links that are currently online.
  base::hash_set<int> online_links_;

  base::Lock is_offline_lock_;
  bool is_offline_;
  bool is_offline_initialized_;
  base::ConditionVariable is_offline_initialized_cv_;
};

}  // namespace internal
}  // namespace net

#endif  // NET_BASE_ADDRESS_TRACKER_LINUX_H_
