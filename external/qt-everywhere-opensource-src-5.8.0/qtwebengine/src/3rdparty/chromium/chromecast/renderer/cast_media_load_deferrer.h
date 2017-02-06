// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_CAST_MEDIA_LOAD_DEFERRER_H_
#define CHROMECAST_RENDERER_CAST_MEDIA_LOAD_DEFERRER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "content/public/renderer/render_frame_observer.h"

namespace chromecast {

// Defers media player loading in background web apps until brought into
// foreground.
class CastMediaLoadDeferrer : public content::RenderFrameObserver {
 public:
  // Will run |closure| to continue loading the media resource once the page is
  // swapped in.
  CastMediaLoadDeferrer(content::RenderFrame* render_frame,
                        const base::Closure& continue_loading_cb);
  ~CastMediaLoadDeferrer() override;

 private:
  // content::RenderFrameObserver implementation:
  void WasShown() override;
  void OnDestruct() override;

  base::Closure continue_loading_cb_;

  DISALLOW_COPY_AND_ASSIGN(CastMediaLoadDeferrer);
};

}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_CAST_MEDIA_LOAD_DEFERRER_H_
