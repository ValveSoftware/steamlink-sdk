// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_P2P_IPC_NETWORK_MANAGER_H_
#define CONTENT_RENDERER_P2P_IPC_NETWORK_MANAGER_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/renderer/p2p/network_list_observer.h"
#include "content/renderer/p2p/socket_dispatcher.h"
#include "third_party/libjingle/source/talk/base/network.h"

namespace content {

// IpcNetworkManager is a NetworkManager for libjingle that gets a
// list of network interfaces from the browser.
class IpcNetworkManager : public talk_base::NetworkManagerBase,
                          public NetworkListObserver {
 public:
  // Constructor doesn't take ownership of the |socket_dispatcher|.
  CONTENT_EXPORT IpcNetworkManager(P2PSocketDispatcher* socket_dispatcher);
  virtual ~IpcNetworkManager();

  virtual void StartUpdating() OVERRIDE;
  virtual void StopUpdating() OVERRIDE;

  // P2PSocketDispatcher::NetworkListObserver interface.
  virtual void OnNetworkListChanged(
      const net::NetworkInterfaceList& list) OVERRIDE;

 private:
  void SendNetworksChangedSignal();

  P2PSocketDispatcher* socket_dispatcher_;
  int start_count_;
  bool network_list_received_;

  base::WeakPtrFactory<IpcNetworkManager> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_P2P_IPC_NETWORK_MANAGER_H_
