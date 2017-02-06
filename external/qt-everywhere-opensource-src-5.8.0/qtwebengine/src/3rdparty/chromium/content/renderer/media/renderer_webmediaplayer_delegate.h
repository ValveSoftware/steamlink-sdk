// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_RENDERER_WEBMEDIAPLAYER_DELEGATE_H_
#define CONTENT_RENDERER_MEDIA_RENDERER_WEBMEDIAPLAYER_DELEGATE_H_

#include <map>
#include <memory>

#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/default_tick_clock.h"
#include "base/timer/timer.h"
#include "content/common/content_export.h"
#include "content/public/renderer/render_frame_observer.h"
#include "media/blink/webmediaplayer_delegate.h"

namespace blink {
class WebMediaPlayer;
}

namespace media {

// An interface to allow a WebMediaPlayerImpl to communicate changes of state
// to objects that need to know.
class CONTENT_EXPORT RendererWebMediaPlayerDelegate
    : public content::RenderFrameObserver,
      public NON_EXPORTED_BASE(WebMediaPlayerDelegate),
      public NON_EXPORTED_BASE(
          base::SupportsWeakPtr<RendererWebMediaPlayerDelegate>) {
 public:
  explicit RendererWebMediaPlayerDelegate(content::RenderFrame* render_frame);
  ~RendererWebMediaPlayerDelegate() override;

  // Returns true if this RenderFrame has ever seen media playback before.
  bool has_played_media() const { return has_played_media_; }

  // WebMediaPlayerDelegate implementation.
  int AddObserver(Observer* observer) override;
  void RemoveObserver(int delegate_id) override;
  void DidPlay(int delegate_id,
               bool has_video,
               bool has_audio,
               bool is_remote,
               base::TimeDelta duration) override;
  void DidPause(int delegate_id, bool reached_end_of_stream) override;
  void PlayerGone(int delegate_id) override;
  bool IsHidden() override;

  // content::RenderFrameObserver overrides.
  void WasHidden() override;
  void WasShown() override;
  bool OnMessageReceived(const IPC::Message& msg) override;
  void OnDestruct() override;

  // Zeros out |idle_cleanup_interval_|, and sets |idle_timeout_| to
  // |idle_timeout|. A zero cleanup interval will cause the idle timer to run
  // with each run of the message loop.
  void SetIdleCleanupParamsForTesting(base::TimeDelta idle_timeout,
                                      base::TickClock* tick_clock);
  bool IsIdleCleanupTimerRunningForTesting() const {
    return idle_cleanup_timer_.IsRunning();
  }

 private:
  void OnMediaDelegatePause(int delegate_id);
  void OnMediaDelegatePlay(int delegate_id);
  void OnMediaDelegateSuspendAllMediaPlayers();
  void OnMediaDelegateVolumeMultiplierUpdate(int delegate_id,
                                             double multiplier);

  // Adds or removes a delegate from |idle_delegate_map_|. The first insertion
  // or last removal will start or stop |idle_cleanup_timer_| respectively.
  void AddIdleDelegate(int delegate_id);
  void RemoveIdleDelegate(int delegate_id);

  // Runs periodically to suspend idle delegates in |idle_delegate_map_|.
  void CleanupIdleDelegates();

  bool has_played_media_ = false;
  IDMap<Observer> id_map_;

  // Tracks which delegates have entered an idle state. After some period of
  // inactivity these players will be suspended to release unused resources.
  bool idle_cleanup_running_ = false;
  std::map<int, base::TimeTicks> idle_delegate_map_;
  base::RepeatingTimer idle_cleanup_timer_;

  // Amount of time allowed to elapse after a delegate enters the paused before
  // the delegate is suspended.
  base::TimeDelta idle_timeout_;

  // The polling interval used for checking the delegates to see if any have
  // exceeded |idle_timeout_| since their last pause state.
  base::TimeDelta idle_cleanup_interval_;

  // Clock used for calculating when delegates have expired. May be overridden
  // for testing.
  std::unique_ptr<base::DefaultTickClock> default_tick_clock_;
  base::TickClock* tick_clock_;

  DISALLOW_COPY_AND_ASSIGN(RendererWebMediaPlayerDelegate);
};

}  // namespace media

#endif  // CONTENT_RENDERER_MEDIA_RENDERER_WEBMEDIAPLAYER_DELEGATE_H_
