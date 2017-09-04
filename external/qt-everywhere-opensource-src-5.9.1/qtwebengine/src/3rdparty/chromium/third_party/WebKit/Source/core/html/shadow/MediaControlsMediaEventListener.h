// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlsMediaEventListener_h
#define MediaControlsMediaEventListener_h

#include "core/events/EventListener.h"

namespace blink {

class MediaControls;

class MediaControlsMediaEventListener final : public EventListener {
 public:
  explicit MediaControlsMediaEventListener(MediaControls*);

  bool operator==(const EventListener&) const override;

  DECLARE_VIRTUAL_TRACE();

 private:
  void handleEvent(ExecutionContext*, Event*) override;

  Member<MediaControls> m_mediaControls;
};

}  // namespace blink

#endif  // MediaControlsMediaEventListener_h
