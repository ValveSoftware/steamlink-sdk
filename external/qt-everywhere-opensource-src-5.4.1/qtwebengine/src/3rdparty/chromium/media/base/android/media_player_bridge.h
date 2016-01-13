// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_PLAYER_BRIDGE_H_
#define MEDIA_BASE_ANDROID_MEDIA_PLAYER_BRIDGE_H_

#include <jni.h>
#include <map>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "media/base/android/media_player_android.h"
#include "media/base/android/media_player_listener.h"
#include "url/gurl.h"

namespace media {

class MediaPlayerManager;

// This class serves as a bridge between the native code and Android MediaPlayer
// Java class. For more information on Android MediaPlayer, check
// http://developer.android.com/reference/android/media/MediaPlayer.html
// The actual Android MediaPlayer instance is created lazily when Start(),
// Pause(), SeekTo() gets called. As a result, media information may not
// be available until one of those operations is performed. After that, we
// will cache those information in case the mediaplayer gets released.
// The class uses the corresponding MediaPlayerBridge Java class to talk to
// the Android MediaPlayer instance.
class MEDIA_EXPORT MediaPlayerBridge : public MediaPlayerAndroid {
 public:
  static bool RegisterMediaPlayerBridge(JNIEnv* env);

  // Construct a MediaPlayerBridge object. This object needs to call |manager|'s
  // RequestMediaResources() before decoding the media stream. This allows
  // |manager| to track unused resources and free them when needed. On the other
  // hand, it needs to call ReleaseMediaResources() when it is done with
  // decoding. MediaPlayerBridge also forwards Android MediaPlayer callbacks to
  // the |manager| when needed.
  MediaPlayerBridge(int player_id,
                    const GURL& url,
                    const GURL& first_party_for_cookies,
                    const std::string& user_agent,
                    bool hide_url_log,
                    MediaPlayerManager* manager,
                    const RequestMediaResourcesCB& request_media_resources_cb,
                    const ReleaseMediaResourcesCB& release_media_resources_cb,
                    const GURL& frame_url,
                    bool allow_credentials);
  virtual ~MediaPlayerBridge();

  // Initialize this object and extract the metadata from the media.
  virtual void Initialize();

  // MediaPlayerAndroid implementation.
  virtual void SetVideoSurface(gfx::ScopedJavaSurface surface) OVERRIDE;
  virtual void Start() OVERRIDE;
  virtual void Pause(bool is_media_related_action ALLOW_UNUSED) OVERRIDE;
  virtual void SeekTo(base::TimeDelta timestamp) OVERRIDE;
  virtual void Release() OVERRIDE;
  virtual void SetVolume(double volume) OVERRIDE;
  virtual int GetVideoWidth() OVERRIDE;
  virtual int GetVideoHeight() OVERRIDE;
  virtual base::TimeDelta GetCurrentTime() OVERRIDE;
  virtual base::TimeDelta GetDuration() OVERRIDE;
  virtual bool IsPlaying() OVERRIDE;
  virtual bool CanPause() OVERRIDE;
  virtual bool CanSeekForward() OVERRIDE;
  virtual bool CanSeekBackward() OVERRIDE;
  virtual bool IsPlayerReady() OVERRIDE;
  virtual GURL GetUrl() OVERRIDE;
  virtual GURL GetFirstPartyForCookies() OVERRIDE;
  virtual bool IsSurfaceInUse() const OVERRIDE;

  // MediaPlayerListener callbacks.
  void OnVideoSizeChanged(int width, int height);
  void OnMediaError(int error_type);
  void OnBufferingUpdate(int percent);
  void OnPlaybackComplete();
  void OnMediaInterrupted();
  void OnSeekComplete();
  void OnDidSetDataUriDataSource(JNIEnv* env, jobject obj, jboolean success);

 protected:
  void SetJavaMediaPlayerBridge(jobject j_media_player_bridge);
  base::android::ScopedJavaLocalRef<jobject> GetJavaMediaPlayerBridge();
  void SetMediaPlayerListener();
  void SetDuration(base::TimeDelta time);

  virtual void PendingSeekInternal(const base::TimeDelta& time);

  // Prepare the player for playback, asynchronously. When succeeds,
  // OnMediaPrepared() will be called. Otherwise, OnMediaError() will
  // be called with an error type.
  virtual void Prepare();
  void OnMediaPrepared();

  // Create the corresponding Java class instance.
  virtual void CreateJavaMediaPlayerBridge();

  // Get allowed operations from the player.
  virtual base::android::ScopedJavaLocalRef<jobject> GetAllowedOperations();

 private:
  friend class MediaPlayerListener;

  // Set the data source for the media player.
  void SetDataSource(const std::string& url);

  // Functions that implements media player control.
  void StartInternal();
  void PauseInternal();
  void SeekInternal(base::TimeDelta time);

  // Called when |time_update_timer_| fires.
  void OnTimeUpdateTimerFired();

  // Update allowed operations from the player.
  void UpdateAllowedOperations();

  // Callback function passed to |resource_getter_|. Called when the cookies
  // are retrieved.
  void OnCookiesRetrieved(const std::string& cookies);

  // Extract the media metadata from a url, asynchronously.
  // OnMediaMetadataExtracted() will be called when this call finishes.
  void ExtractMediaMetadata(const std::string& url);
  void OnMediaMetadataExtracted(base::TimeDelta duration, int width, int height,
                                bool success);

  // Returns true if a MediaUrlInterceptor registered by the embedder has
  // intercepted the url.
  bool InterceptMediaUrl(
      const std::string& url, int* fd, int64* offset, int64* size);

  // Whether the player is prepared for playback.
  bool prepared_;

  // Pending play event while player is preparing.
  bool pending_play_;

  // Pending seek time while player is preparing.
  base::TimeDelta pending_seek_;

  // Url for playback.
  GURL url_;

  // First party url for cookies.
  GURL first_party_for_cookies_;

  // User agent string to be used for media player.
  const std::string user_agent_;

  // Hide url log from media player.
  bool hide_url_log_;

  // Stats about the media.
  base::TimeDelta duration_;
  int width_;
  int height_;

  // Meta data about actions can be taken.
  bool can_pause_;
  bool can_seek_forward_;
  bool can_seek_backward_;

  // Cookies for |url_|.
  std::string cookies_;

  // Java MediaPlayerBridge instance.
  base::android::ScopedJavaGlobalRef<jobject> j_media_player_bridge_;

  base::RepeatingTimer<MediaPlayerBridge> time_update_timer_;

  // Listener object that listens to all the media player events.
  scoped_ptr<MediaPlayerListener> listener_;

  // Whether player is currently using a surface.
  bool is_surface_in_use_;

  // Volume of playback.
  double volume_;

  // Whether user credentials are allowed to be passed.
  bool allow_credentials_;

  // Weak pointer passed to |listener_| for callbacks.
  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MediaPlayerBridge> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MediaPlayerBridge);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_PLAYER_BRIDGE_H_
