// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/mock_media_session_observer.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace content {

MockMediaSessionObserver::MockMediaSessionObserver() = default;

MockMediaSessionObserver::~MockMediaSessionObserver() = default;

void MockMediaSessionObserver::OnSuspend(int player_id) {
  EXPECT_GE(player_id, 0);
  EXPECT_GT(players_.size(), static_cast<size_t>(player_id));

  ++received_suspend_calls_;
  players_[player_id].is_playing_ = false;
}

void MockMediaSessionObserver::OnResume(int player_id) {
  EXPECT_GE(player_id, 0);
  EXPECT_GT(players_.size(), static_cast<size_t>(player_id));

  ++received_resume_calls_;
  players_[player_id].is_playing_ = true;
}

void MockMediaSessionObserver::OnSetVolumeMultiplier(
    int player_id, double volume_multiplier) {
  EXPECT_GE(player_id, 0);
  EXPECT_GT(players_.size(), static_cast<size_t>(player_id));

  EXPECT_GE(volume_multiplier, 0.0f);
  EXPECT_LE(volume_multiplier, 1.0f);

  players_[player_id].volume_multiplier_ = volume_multiplier;
}

int MockMediaSessionObserver::StartNewPlayer() {
  players_.push_back(MockPlayer(true, 1.0f));
  return players_.size() - 1;
}

bool MockMediaSessionObserver::IsPlaying(size_t player_id) {
  EXPECT_GT(players_.size(), player_id);
  return players_[player_id].is_playing_;
}

double MockMediaSessionObserver::GetVolumeMultiplier(size_t player_id) {
  EXPECT_GT(players_.size(), player_id);
  return players_[player_id].volume_multiplier_;
}

void MockMediaSessionObserver::SetPlaying(size_t player_id, bool playing) {
  EXPECT_GT(players_.size(), player_id);
  players_[player_id].is_playing_ = playing;
}

int MockMediaSessionObserver::received_suspend_calls() const {
  return received_suspend_calls_;
}

int MockMediaSessionObserver::received_resume_calls() const {
  return received_resume_calls_;
}

}  // namespace content
