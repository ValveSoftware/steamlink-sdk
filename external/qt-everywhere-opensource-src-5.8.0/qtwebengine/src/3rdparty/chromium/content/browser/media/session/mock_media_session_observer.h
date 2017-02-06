// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <vector>

#include "content/browser/media/session/media_session_observer.h"

namespace content {

// MockMediaSessionObserver is a mock implementation of MediaSessionObserver to
// be used in tests.
class MockMediaSessionObserver : public MediaSessionObserver {
 public:
  MockMediaSessionObserver();
  ~MockMediaSessionObserver() override;

  // Implements MediaSessionObserver.
  void OnSuspend(int player_id) override;
  void OnResume(int player_id) override;
  void OnSetVolumeMultiplier(int player_id, double volume_multiplier) override;

  // Simulate that a new player started.
  // Returns the player_id.
  int StartNewPlayer();

  // Returns whether |player_id| is playing.
  bool IsPlaying(size_t player_id);

  // Returns the volume multiplier of |player_id|.
  double GetVolumeMultiplier(size_t player_id);

  // Simulate a play state change for |player_id|.
  void SetPlaying(size_t player_id, bool playing);

  int received_suspend_calls() const;
  int received_resume_calls() const;

 private:
  // Internal representation of the players to keep track of their statuses.
  struct MockPlayer {
   public:
    MockPlayer(bool is_playing = true, double volume_multiplier = 1.0f)
        : is_playing_(is_playing),
          volume_multiplier_(volume_multiplier) {}
    bool is_playing_;
    double volume_multiplier_;
  };

  // Basic representation of the players. The position in the vector is the
  // player_id. The value of the vector is the playing status and volume.
  std::vector<MockPlayer> players_;

  int received_resume_calls_ = 0;
  int received_suspend_calls_ = 0;
};

}  // namespace content
