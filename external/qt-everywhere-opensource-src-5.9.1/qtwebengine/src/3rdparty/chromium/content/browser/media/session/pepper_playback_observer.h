// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_SESSION_PEPPER_PLAYBACK_OBSERVER_H_
#define CONTENT_BROWSER_MEDIA_SESSION_PEPPER_PLAYBACK_OBSERVER_H_

#include <stdint.h>
#include <map>
#include <memory>

#include "base/macros.h"

namespace content {

class PepperPlayerDelegate;
class WebContentsImpl;

// Class observing Pepper playback changes from WebContents, and update
// MediaSession accordingly. Can only be a member of WebContentsImpl and must be
// destroyed in ~WebContentsImpl().
class PepperPlaybackObserver {
 public:
  explicit PepperPlaybackObserver(WebContentsImpl* contents);
  virtual ~PepperPlaybackObserver();

  void PepperInstanceCreated(int32_t pp_instance);
  void PepperInstanceDeleted(int32_t pp_instance);
  // This method is called when a Pepper instance starts making sound.
  void PepperStartsPlayback(int32_t pp_instance);
  // This method is called when a Pepper instance stops making sound.
  void PepperStopsPlayback(int32_t pp_instance);

 private:
  // Owning PepperPlayerDelegates.
  using PlayersMap =
      std::map<int32_t, std::unique_ptr<PepperPlayerDelegate>>;
  PlayersMap players_map_;

  // Map for whether Pepper players have ever played sound.
  // Used for recording UMA.
  using PlayersPlayedSoundMap =
      std::map<int32_t, bool>;
  PlayersPlayedSoundMap players_played_sound_map_;

  // Weak reference to WebContents.
  WebContentsImpl* contents_;

  DISALLOW_COPY_AND_ASSIGN(PepperPlaybackObserver);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_SESSION_PEPPER_PLAYBACK_OBSERVER_H_
