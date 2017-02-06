// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_CAST_OVERLAY_MANAGER_CAST_H_
#define UI_OZONE_PLATFORM_CAST_OVERLAY_MANAGER_CAST_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "chromecast/public/graphics_types.h"
#include "chromecast/public/video_plane.h"
#include "ui/ozone/ozone_export.h"
#include "ui/ozone/public/overlay_manager_ozone.h"

namespace ui {

class OZONE_EXPORT OverlayManagerCast : public OverlayManagerOzone {
 public:
  OverlayManagerCast();
  ~OverlayManagerCast() override;

  // OverlayManagerOzone:
  std::unique_ptr<OverlayCandidatesOzone> CreateOverlayCandidates(
      gfx::AcceleratedWidget w) override;

  // Callback that's made whenever an overlay quad is processed
  // in the compositor.  Used to allow hardware video plane to
  // be positioned to match compositor hole.
  using OverlayCompositedCallback =
      base::Callback<void(const chromecast::RectF&,
                          chromecast::media::VideoPlane::Transform)>;
  static void SetOverlayCompositedCallback(const OverlayCompositedCallback& cb);

 private:

  DISALLOW_COPY_AND_ASSIGN(OverlayManagerCast);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_CAST_OVERLAY_MANAGER_CAST_H_
