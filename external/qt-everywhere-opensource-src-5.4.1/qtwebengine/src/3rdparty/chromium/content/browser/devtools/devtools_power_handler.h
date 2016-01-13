// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_POWER_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_POWER_HANDLER_H_

#include "base/time/time.h"
#include "content/browser/devtools/devtools_protocol.h"
#include "content/browser/power_profiler/power_profiler_observer.h"

namespace content {

// This class provides power information to DevTools.
class DevToolsPowerHandler
    : public DevToolsProtocol::Handler,
      public PowerProfilerObserver {
 public:
  DevToolsPowerHandler();
  virtual ~DevToolsPowerHandler();

  // PowerProfilerObserver override.
  virtual void OnPowerEvent(const PowerEventVector&) OVERRIDE;

 private:
  scoped_refptr<DevToolsProtocol::Response> OnStart(
      scoped_refptr<DevToolsProtocol::Command> command);
  scoped_refptr<DevToolsProtocol::Response> OnEnd(
      scoped_refptr<DevToolsProtocol::Command> command);
  scoped_refptr<DevToolsProtocol::Response> OnCanProfilePower(
      scoped_refptr<DevToolsProtocol::Command> command);

  DISALLOW_COPY_AND_ASSIGN(DevToolsPowerHandler);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_DEVTOOLS_POWER_HANDLER_H_
