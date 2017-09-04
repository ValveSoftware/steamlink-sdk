// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURES_FEATURE_CHANNEL_H_
#define EXTENSIONS_COMMON_FEATURES_FEATURE_CHANNEL_H_

#include "base/macros.h"

namespace version_info {
enum class Channel;
}

namespace extensions {

// Gets the current channel as seen by the Feature system.
version_info::Channel GetCurrentChannel();

// Sets the current channel as seen by the Feature system. In the browser
// process this should be chrome::GetChannel(), and in the renderer this will
// need to come from an IPC.
void SetCurrentChannel(version_info::Channel channel);

// Gets the default channel as seen by the Feature system.
version_info::Channel GetDefaultChannel();

// Scoped channel setter. Use for tests.
class ScopedCurrentChannel {
 public:
  explicit ScopedCurrentChannel(version_info::Channel channel);
  ~ScopedCurrentChannel();

 private:
  version_info::Channel original_channel_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCurrentChannel);
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURES_FEATURE_CHANNEL_H_
