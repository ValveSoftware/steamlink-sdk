// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVTOOLS_DISCOVERY_DEVTOOLS_TARGET_DESCRIPTOR_H_
#define COMPONENTS_DEVTOOLS_DISCOVERY_DEVTOOLS_TARGET_DESCRIPTOR_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace content {
class DevToolsAgentHost;
}

namespace devtools_discovery {

// DevToolsTargetDescriptor provides information about devtools target
// and can be used to manipulate the target and query its details.
// Instantiation and discovery of DevToolsTargetDescriptor instances
// is the responsibility of DevToolsDiscoveryManager.
class DevToolsTargetDescriptor {
 public:
  using List = std::vector<DevToolsTargetDescriptor*>;
  virtual ~DevToolsTargetDescriptor() {}

  // Returns the unique target id.
  virtual std::string GetId() const = 0;

  // Returns the id of the parent target, or empty string if no parent.
  virtual std::string GetParentId() const = 0;

  // Returns the target type.
  virtual std::string GetType() const = 0;

  // Returns the target title.
  virtual std::string GetTitle() const = 0;

  // Returns the target description.
  virtual std::string GetDescription() const = 0;

  // Returns the url associated with this target.
  virtual GURL GetURL() const = 0;

  // Returns the favicon url for this target.
  virtual GURL GetFaviconURL() const = 0;

  // Returns the time when the target was last active.
  virtual base::TimeTicks GetLastActivityTime() const = 0;

  // Returns true if the debugger is attached to the target.
  virtual bool IsAttached() const = 0;

  // Returns the agent host for this target.
  virtual scoped_refptr<content::DevToolsAgentHost> GetAgentHost() const = 0;

  // Activates the target. Returns false if the operation failed.
  virtual bool Activate() const = 0;

  // Closes the target. Returns false if the operation failed.
  virtual bool Close() const = 0;
};

}  // namespace devtools_discovery

#endif  // COMPONENTS_DEVTOOLS_DISCOVERY_DEVTOOLS_TARGET_DESCRIPTOR_H_
