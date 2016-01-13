// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_PLAYER_MANAGER_H_
#define MEDIA_BASE_ANDROID_MEDIA_PLAYER_MANAGER_H_

#include "base/basictypes.h"
#include "base/time/time.h"
#include "media/base/android/demuxer_stream_player_params.h"
#include "media/base/media_export.h"

namespace media {

class MediaPlayerAndroid;
class MediaResourceGetter;
class MediaUrlInterceptor;

// This class is responsible for managing active MediaPlayerAndroid objects.
class MEDIA_EXPORT MediaPlayerManager {
 public:
  virtual ~MediaPlayerManager() {}

  // Returns a pointer to the MediaResourceGetter object.
  virtual MediaResourceGetter* GetMediaResourceGetter() = 0;

  // Returns a pointer to the MediaUrlInterceptor object or null.
  virtual MediaUrlInterceptor* GetMediaUrlInterceptor() = 0;

  // Called when time update messages need to be sent. Args: player ID,
  // current time.
  virtual void OnTimeUpdate(int player_id, base::TimeDelta current_time) = 0;

  // Called when media metadata changed. Args: player ID, duration of the
  // media, width, height, whether the metadata is successfully extracted.
  virtual void OnMediaMetadataChanged(
      int player_id,
      base::TimeDelta duration,
      int width,
      int height,
      bool success) = 0;

  // Called when playback completed. Args: player ID.
  virtual void OnPlaybackComplete(int player_id) = 0;

  // Called when media download was interrupted. Args: player ID.
  virtual void OnMediaInterrupted(int player_id) = 0;

  // Called when buffering has changed. Args: player ID, percentage
  // of the media.
  virtual void OnBufferingUpdate(int player_id, int percentage) = 0;

  // Called when seek completed. Args: player ID, current time.
  virtual void OnSeekComplete(
      int player_id,
      const base::TimeDelta& current_time) = 0;

  // Called when error happens. Args: player ID, error type.
  virtual void OnError(int player_id, int error) = 0;

  // Called when video size has changed. Args: player ID, width, height.
  virtual void OnVideoSizeChanged(int player_id, int width, int height) = 0;

  // Returns the player that's in the fullscreen mode currently.
  virtual MediaPlayerAndroid* GetFullscreenPlayer() = 0;

  // Returns the player with the specified id.
  virtual MediaPlayerAndroid* GetPlayer(int player_id) = 0;

  // Called by the player to get a hardware protected surface.
  virtual void RequestFullScreen(int player_id) = 0;

#if defined(VIDEO_HOLE)
  // Returns true if a media player should use video-overlay for the embedded
  // encrypted video.
  virtual bool ShouldUseVideoOverlayForEmbeddedEncryptedVideo() = 0;
#endif  // defined(VIDEO_HOLE)
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_PLAYER_MANAGER_H_
