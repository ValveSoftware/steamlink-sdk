// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_OBSERVER_H_
#define MEDIA_BASE_MEDIA_OBSERVER_H_

#include "media/base/cdm_context.h"
#include "media/base/pipeline_metadata.h"

namespace media {

// This class is an observer of media player events.
class MEDIA_EXPORT MediaObserver {
 public:
  MediaObserver();
  virtual ~MediaObserver();

  // Called when the media element entered/exited fullscreen.
  virtual void OnEnteredFullscreen() = 0;
  virtual void OnExitedFullscreen() = 0;

  // Called when CDM is attached to the media element. The |cdm_context| is
  // only guaranteed to be valid in this call.
  virtual void OnSetCdm(CdmContext* cdm_context) = 0;

  // Called after demuxer is initialized.
  virtual void OnMetadataChanged(const PipelineMetadata& metadata) = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_OBSERVER_H_
