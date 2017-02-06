// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_network_delegate.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "chromecast/base/chromecast_switches.h"

namespace chromecast {
namespace shell {

CastNetworkDelegate::CastNetworkDelegate() {
  DetachFromThread();
}

CastNetworkDelegate::~CastNetworkDelegate() {
}

bool CastNetworkDelegate::OnCanAccessFile(const net::URLRequest& request,
                                          const base::FilePath& path) const {
  if (base::CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kEnableLocalFileAccesses)) {
    return true;
  }

  LOG(WARNING) << "Could not access file " << path.value()
               << ". All file accesses are forbidden.";
  return false;
}

}  // namespace shell
}  // namespace chromecast
