// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_UTILITY_CONTENT_UTILITY_CLIENT_H_
#define CONTENT_PUBLIC_UTILITY_CONTENT_UTILITY_CLIENT_H_

#include <map>
#include <memory>

#include "base/callback_forward.h"
#include "content/public/common/content_client.h"
#include "content/public/common/service_info.h"

namespace service_manager {
class InterfaceRegistry;
class Service;
}

namespace content {

// Embedder API for participating in renderer logic.
class CONTENT_EXPORT ContentUtilityClient {
 public:
  using StaticServiceMap = std::map<std::string, ServiceInfo>;

  virtual ~ContentUtilityClient() {}

  // Notifies us that the UtilityThread has been created.
  virtual void UtilityThreadStarted() {}

  // Allows the embedder to filter messages.
  virtual bool OnMessageReceived(const IPC::Message& message);

  // Allows the client to expose interfaces from this utility process to the
  // browser process via |registry|.
  virtual void ExposeInterfacesToBrowser(
      service_manager::InterfaceRegistry* registry) {}

  virtual void RegisterServices(StaticServiceMap* services) {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_UTILITY_CONTENT_UTILITY_CLIENT_H_
