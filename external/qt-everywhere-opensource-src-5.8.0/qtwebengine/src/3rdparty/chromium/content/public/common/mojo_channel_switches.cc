// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/mojo_channel_switches.h"

namespace switches {

// The token to use to construct the message pipe on which to layer ChannelMojo.
const char kMojoChannelToken[] = "mojo-channel-token";

// The token to use to construct the message pipe for MojoApplication.
const char kMojoApplicationChannelToken[] = "mojo-application-channel-token";

}  // namespace switches
