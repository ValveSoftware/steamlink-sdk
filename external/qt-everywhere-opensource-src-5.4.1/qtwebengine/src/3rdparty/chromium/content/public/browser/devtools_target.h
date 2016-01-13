// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_TARGET_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_TARGET_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

class DevToolsAgentHost;

// DevToolsTarget represents an inspectable target and can be used to
// manipulate the target and query its details.
// Instantiation and discovery of DevToolsTarget instances is the responsibility
// of DevToolsHttpHandlerDelegate.
class DevToolsTarget {
 public:
  virtual ~DevToolsTarget() {}

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
  virtual scoped_refptr<DevToolsAgentHost> GetAgentHost() const = 0;

  // Activates the target. Returns false if the operation failed.
  virtual bool Activate() const = 0;

  // Closes the target. Returns false if the operation failed.
  virtual bool Close() const = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_TARGET_H_
