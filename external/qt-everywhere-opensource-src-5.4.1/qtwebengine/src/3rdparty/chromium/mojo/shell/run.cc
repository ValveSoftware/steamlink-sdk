// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/run.h"

#include "base/logging.h"
#include "mojo/service_manager/service_manager.h"
#include "mojo/shell/context.h"
#include "mojo/shell/keep_alive.h"

namespace mojo {
namespace shell {

void Run(Context* context, const std::vector<GURL>& app_urls) {
  KeepAlive keep_alive(context);

  if (app_urls.empty()) {
    LOG(ERROR) << "No app path specified";
    return;
  }

  for (std::vector<GURL>::const_iterator it = app_urls.begin();
       it != app_urls.end();
       ++it) {
    ScopedMessagePipeHandle no_handle;
    context->service_manager()->ConnectToService(
        *it, std::string(), no_handle.Pass(), GURL());
  }
}

}  // namespace shell
}  // namespace mojo
