// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_DEVTOOLS_REMOTE_DEBUGGING_SERVER_H_
#define CHROMECAST_BROWSER_DEVTOOLS_REMOTE_DEBUGGING_SERVER_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "components/prefs/pref_member.h"

namespace devtools_http_handler {
class DevToolsHttpHandler;
}

namespace chromecast {
namespace shell {

class CastDevToolsManagerDelegate;

class RemoteDebuggingServer {
 public:
  explicit RemoteDebuggingServer(bool start_immediately);
  ~RemoteDebuggingServer();

 private:
  // Called when pref_enabled_ is changed.
  void OnEnabledChanged();

  std::unique_ptr<devtools_http_handler::DevToolsHttpHandler>
      devtools_http_handler_;

  BooleanPrefMember pref_enabled_;
  uint16_t port_;

  DISALLOW_COPY_AND_ASSIGN(RemoteDebuggingServer);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_DEVTOOLS_REMOTE_DEBUGGING_SERVER_H_
