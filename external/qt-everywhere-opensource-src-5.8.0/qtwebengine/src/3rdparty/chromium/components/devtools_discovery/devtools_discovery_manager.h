// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVTOOLS_DISCOVERY_DEVTOOLS_DISCOVERY_MANAGER_H_
#define COMPONENTS_DEVTOOLS_DISCOVERY_DEVTOOLS_DISCOVERY_MANAGER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/devtools_discovery/devtools_target_descriptor.h"

namespace devtools_discovery {

class DevToolsDiscoveryManager {
 public:
  class Provider {
   public:
    virtual ~Provider() {}

    // Caller takes ownership of created descriptors.
    virtual DevToolsTargetDescriptor::List GetDescriptors() = 0;
  };

  using CreateCallback =
      base::Callback<std::unique_ptr<DevToolsTargetDescriptor>(
          const GURL& url)>;

  // Returns single instance of this class. The instance is destroyed on the
  // browser main loop exit so this method MUST NOT be called after that point.
  static DevToolsDiscoveryManager* GetInstance();

  void AddProvider(std::unique_ptr<Provider> provider);
  void SetCreateCallback(const CreateCallback& callback);

  // Caller takes ownership of created descriptors.
  DevToolsTargetDescriptor::List GetDescriptors();
  std::unique_ptr<DevToolsTargetDescriptor> CreateNew(const GURL& url);

 private:
  friend struct base::DefaultSingletonTraits<DevToolsDiscoveryManager>;

  DevToolsDiscoveryManager();
  ~DevToolsDiscoveryManager();
  DevToolsTargetDescriptor::List GetDescriptorsFromProviders();

  std::vector<Provider*> providers_;
  CreateCallback create_callback_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsDiscoveryManager);
};

}  // namespace devtools_discovery

#endif  // COMPONENTS_DEVTOOLS_DISCOVERY_DEVTOOLS_DISCOVERY_MANAGER_H_
