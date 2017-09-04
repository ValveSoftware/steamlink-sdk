// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SYSTEM_INFO_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SYSTEM_INFO_HANDLER_H_

#include <set>

#include "base/macros.h"
#include "content/browser/devtools/protocol/devtools_protocol_dispatcher.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_data_manager_observer.h"

namespace content {
namespace devtools {
namespace system_info {

class SystemInfoHandler {
 public:
  using Response = DevToolsProtocolClient::Response;

  SystemInfoHandler();
  ~SystemInfoHandler();

  void SetClient(std::unique_ptr<Client> client);

  Response GetInfo(DevToolsCommandId command_id);

 private:
  friend class SystemInfoHandlerGpuObserver;

  void SendGetInfoResponse(DevToolsCommandId command_id);

  // Bookkeeping for the active GpuObservers.
  void AddActiveObserverId(int observer_id);
  bool RemoveActiveObserverId(int observer_id);
  void ObserverWatchdogCallback(int observer_id, DevToolsCommandId command_id);

  // For robustness, we have to guarantee a response to getInfo requests.
  // It's very unlikely that the requests will time out. The
  // GpuDataManager's threading model is not well defined (see comments in
  // gpu_data_manager_impl.h) and it is very difficult to correctly clean
  // up its observers. For the moment, especially since these classes are
  // only used in tests, we leak a little bit of memory if we don't get a
  // callback from the GpuDataManager in time.
  mutable base::Lock lock_;
  std::set<int> active_observers_;

  std::unique_ptr<Client> client_;
  base::WeakPtrFactory<SystemInfoHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SystemInfoHandler);
};

}  // namespace system_info
}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SYSTEM_INFO_HANDLER_H_
