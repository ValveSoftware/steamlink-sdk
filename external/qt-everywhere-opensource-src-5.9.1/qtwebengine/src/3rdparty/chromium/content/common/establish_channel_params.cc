// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/establish_channel_params.h"

namespace content {

EstablishChannelParams::EstablishChannelParams()
    : client_id(0),
      client_tracing_id(0),
      preempts(false),
      allow_view_command_buffers(false),
      allow_real_time_streams(false) {}

EstablishChannelParams::~EstablishChannelParams() {}

}  // namespace content
