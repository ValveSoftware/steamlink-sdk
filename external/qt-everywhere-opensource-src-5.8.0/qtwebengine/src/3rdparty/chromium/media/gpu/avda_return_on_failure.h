// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_AVDA_RETURN_ON_FAILURE_H_
#define MEDIA_GPU_AVDA_RETURN_ON_FAILURE_H_

#include "media/video/video_decode_accelerator.h"

// Helper macros for dealing with failure.  If |result| evaluates false, emit
// |log| to ERROR, register |error| with the decoder, and return.  This will
// also transition to the error state, stopping further decoding.
// This is meant to be used only within AndroidVideoDecoder and the various
// backing strategies.  |provider| must support PostError.  The varargs
// can be used for the return value.
#define RETURN_ON_FAILURE(provider, result, log, error, ...)         \
  do {                                                               \
    if (!(result)) {                                                 \
      DLOG(ERROR) << log;                                            \
      provider->PostError(FROM_HERE, VideoDecodeAccelerator::error); \
      return __VA_ARGS__;                                            \
    }                                                                \
  } while (0)

// Similar to the above, with some handy boilerplate savings.  The varargs
// can be used for the return value.
#define RETURN_IF_NULL(ptr, ...)                                   \
  RETURN_ON_FAILURE(state_provider_, ptr, "Got null for " << #ptr, \
                    ILLEGAL_STATE, ##__VA_ARGS__);

// Return null if !ptr.
#define RETURN_NULL_IF_NULL(ptr) RETURN_IF_NULL(ptr, 0)

#endif  // MEDIA_GPU_AVDA_RETURN_ON_FAILURE_H_
