// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/pepper_playback_observer.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "content/browser/media/session/media_session_impl.h"
#include "content/browser/media/session/pepper_player_delegate.h"
#include "content/common/frame_messages.h"
#include "ipc/ipc_message_macros.h"
#include "media/base/media_content_type.h"
#include "media/base/media_switches.h"

namespace content {

namespace {

bool ShouldDuckFlash() {
  return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
             switches::kEnableDefaultMediaSession) ==
         switches::kEnableDefaultMediaSessionDuckFlash;
}

}  // anonymous namespace

PepperPlaybackObserver::PepperPlaybackObserver(WebContentsImpl *contents)
    : contents_(contents) {}

PepperPlaybackObserver::~PepperPlaybackObserver() {
  // At this point WebContents is being destructed, so it's safe to
  // call this. MediaSession may decide to send further IPC messages
  // through PepperPlayerDelegates, which might be declined if the
  // RenderViewHost has been destroyed.
  for (PlayersMap::iterator iter = players_map_.begin();
       iter != players_map_.end();) {
    MediaSessionImpl::Get(contents_)->RemovePlayer(
        iter->second.get(), PepperPlayerDelegate::kPlayerId);
    iter = players_map_.erase(iter);
  }
}

void PepperPlaybackObserver::PepperInstanceCreated(int32_t pp_instance) {
  players_played_sound_map_[pp_instance] = false;
}

void PepperPlaybackObserver::PepperInstanceDeleted(int32_t pp_instance) {
  UMA_HISTOGRAM_BOOLEAN("Media.Pepper.PlayedSound",
                        players_played_sound_map_[pp_instance]);
  players_played_sound_map_.erase(pp_instance);

  PepperStopsPlayback(pp_instance);
}

void PepperPlaybackObserver::PepperStartsPlayback(int32_t pp_instance) {
  players_played_sound_map_[pp_instance] = true;

  if (players_map_.count(pp_instance))
    return;

  players_map_[pp_instance].reset(new PepperPlayerDelegate(
      contents_, pp_instance));

  MediaSessionImpl::Get(contents_)->AddPlayer(
      players_map_[pp_instance].get(), PepperPlayerDelegate::kPlayerId,
      ShouldDuckFlash() ? media::MediaContentType::Pepper
                        : media::MediaContentType::OneShot);
}

void PepperPlaybackObserver::PepperStopsPlayback(int32_t pp_instance) {
  if (!players_map_.count(pp_instance))
    return;

  MediaSessionImpl::Get(contents_)->RemovePlayer(
      players_map_[pp_instance].get(), PepperPlayerDelegate::kPlayerId);

  players_map_.erase(pp_instance);
}

}  // namespace content
