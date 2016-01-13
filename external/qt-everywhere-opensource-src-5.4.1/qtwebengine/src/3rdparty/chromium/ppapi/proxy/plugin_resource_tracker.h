// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PLUGIN_RESOURCE_TRACKER_H_
#define PPAPI_PROXY_PLUGIN_RESOURCE_TRACKER_H_

#include <map>
#include <utility>

#include "base/compiler_specific.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/host_resource.h"
#include "ppapi/shared_impl/resource_tracker.h"

template<typename T> struct DefaultSingletonTraits;

namespace ppapi {

namespace proxy {

class PPAPI_PROXY_EXPORT PluginResourceTracker : public ResourceTracker {
 public:
  PluginResourceTracker();
  virtual ~PluginResourceTracker();

  // Given a host resource, maps it to an existing plugin resource ID if it
  // exists, or returns 0 on failure.
  PP_Resource PluginResourceForHostResource(
      const HostResource& resource) const;

 protected:
  // ResourceTracker overrides.
  virtual PP_Resource AddResource(Resource* object) OVERRIDE;
  virtual void RemoveResource(Resource* object) OVERRIDE;

 private:
  // Map of host instance/resource pairs to a plugin resource ID.
  typedef std::map<HostResource, PP_Resource> HostResourceMap;
  HostResourceMap host_resource_map_;

  DISALLOW_COPY_AND_ASSIGN(PluginResourceTracker);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PLUGIN_RESOURCE_TRACKER_H_
