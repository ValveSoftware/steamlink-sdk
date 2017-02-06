// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/cast_media_load_deferrer.h"

#include "base/callback_helpers.h"
#include "base/logging.h"

namespace chromecast {

CastMediaLoadDeferrer::CastMediaLoadDeferrer(
    content::RenderFrame* render_frame,
    const base::Closure& continue_loading_cb)
    : content::RenderFrameObserver(render_frame),
      continue_loading_cb_(continue_loading_cb) {
  DCHECK(!continue_loading_cb_.is_null());
}

CastMediaLoadDeferrer::~CastMediaLoadDeferrer() {}

void CastMediaLoadDeferrer::WasShown() {
  continue_loading_cb_.Run();
  delete this;
}

void CastMediaLoadDeferrer::OnDestruct() {
  delete this;
}

}  // namespace chromecast
