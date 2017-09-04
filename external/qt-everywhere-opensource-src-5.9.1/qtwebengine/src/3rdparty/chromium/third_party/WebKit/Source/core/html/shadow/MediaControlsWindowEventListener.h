// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaControlsWindowEventListener_h
#define MediaControlsWindowEventListener_h

#include "core/events/EventListener.h"
#include "wtf/Functional.h"

namespace blink {

class MediaControls;

class MediaControlsWindowEventListener final : public EventListener {
 public:
  using Callback = Function<void(), WTF::SameThreadAffinity>;

  static MediaControlsWindowEventListener* create(MediaControls*,
                                                  std::unique_ptr<Callback>);

  bool operator==(const EventListener&) const override;

  void start();
  void stop();

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit MediaControlsWindowEventListener(MediaControls*,
                                            std::unique_ptr<Callback>);

  void handleEvent(ExecutionContext*, Event*) override;

  Member<MediaControls> m_mediaControls;
  std::unique_ptr<Callback> m_callback;
  bool m_isActive;
};

}  // namespace blink

#endif  // MediaControlsWindowEventListener_h
