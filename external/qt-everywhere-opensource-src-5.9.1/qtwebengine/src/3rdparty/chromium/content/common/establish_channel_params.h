// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_ESTABLISH_CHANNEL_PARAMS_H_
#define CONTENT_COMMON_ESTABLISH_CHANNEL_PARAMS_H_

#include <stdint.h>

#include "content/common/content_export.h"

namespace content {

struct CONTENT_EXPORT EstablishChannelParams {
  EstablishChannelParams();
  ~EstablishChannelParams();

  int client_id;
  uint64_t client_tracing_id;
  bool preempts;
  bool allow_view_command_buffers;
  bool allow_real_time_streams;
};

}  // namespace content

#endif  // CONTENT_COMMON_ESTABLISH_CHANNEL_PARAMS_H_
