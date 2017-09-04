// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SURFACE_SEQUENCE_GENERATOR_H_
#define CC_SURFACES_SURFACE_SEQUENCE_GENERATOR_H_

#include <stdint.h>

#include <tuple>

#include "base/macros.h"

#include "cc/surfaces/frame_sink_id.h"

namespace cc {

struct SurfaceSequence;

// Generates unique surface sequences for a surface client id.
class SurfaceSequenceGenerator {
 public:
  SurfaceSequenceGenerator();
  ~SurfaceSequenceGenerator();

  void set_frame_sink_id(const FrameSinkId& frame_sink_id) {
    frame_sink_id_ = frame_sink_id;
  }

  SurfaceSequence CreateSurfaceSequence();

 private:
  FrameSinkId frame_sink_id_;
  uint32_t next_surface_sequence_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceSequenceGenerator);
};

}  // namespace cc

#endif  // CC_SURFACES_SURFACE_SEQUENCE_GENERATOR_H_
