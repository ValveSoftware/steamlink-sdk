// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_COMPOSITOR_FRAME_H_
#define CC_OUTPUT_COMPOSITOR_FRAME_H_

#include <memory>

#include "base/macros.h"
#include "cc/base/cc_export.h"
#include "cc/output/compositor_frame_metadata.h"
#include "cc/output/delegated_frame_data.h"
#include "cc/output/gl_frame_data.h"

namespace cc {

// A CompositorFrame struct contains the complete output of a compositor meant
// for display. A CompositorFrame consists of a series of DrawQuads that
// describe placement of textures, solid colors, overlays and other
// CompositorFrames within an area specified by the parent compositor. DrawQuads
// may share common data referred to as SharedQuadState. A CompositorFrame also
// has |metadata| that refers to global graphical state associated with this
// frame.
class CC_EXPORT CompositorFrame {
 public:
  CompositorFrame();
  CompositorFrame(CompositorFrame&& other);
  ~CompositorFrame();

  CompositorFrame& operator=(CompositorFrame&& other);

  CompositorFrameMetadata metadata;
  std::unique_ptr<DelegatedFrameData> delegated_frame_data;
  std::unique_ptr<GLFrameData> gl_frame_data;

 private:
  DISALLOW_COPY_AND_ASSIGN(CompositorFrame);
};

}  // namespace cc

#endif  // CC_OUTPUT_COMPOSITOR_FRAME_H_
