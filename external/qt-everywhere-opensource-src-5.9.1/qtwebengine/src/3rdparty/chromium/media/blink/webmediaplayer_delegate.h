// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_WEBMEDIAPLAYER_DELEGATE_H_
#define MEDIA_BLINK_WEBMEDIAPLAYER_DELEGATE_H_

namespace blink {
class WebMediaPlayer;
}
namespace media {

enum class MediaContentType;

// An interface to allow a WebMediaPlayer to communicate changes of state to
// objects that need to know.
class WebMediaPlayerDelegate {
 public:
  class Observer {
   public:
    // Called when the WebMediaPlayer enters the background or foreground
    // respectively. Note: Some implementations will stop playback when hidden,
    // and thus subsequently call WebMediaPlayerDelegate::PlayerGone().
    virtual void OnHidden() = 0;
    virtual void OnShown() = 0;

    // Requests a WebMediaPlayer instance to release all idle resources. If
    // |must_suspend| is true, the player must stop playback, release all idle
    // resources, and finally call WebMediaPlayerDelegate::PlayerGone(). If
    // |must_suspend| is false, the player may ignore the request. Optionally,
    // it may do some or all of the same actions as when |must_suspend| is true.
    // To be clear, the player is not required to call PlayerGone() when
    // |must_suspend| is false.
    // Return false to reject the request and indicate that further calls to
    // OnSuspendRequested() are required. Otherwise the Observer is removed
    // from the idle list.
    virtual bool OnSuspendRequested(bool must_suspend) = 0;

    virtual void OnPlay() = 0;
    virtual void OnPause() = 0;

    // Playout volume should be set to current_volume * multiplier. The range is
    // [0, 1] and is typically 1.
    virtual void OnVolumeMultiplierUpdate(double multiplier) = 0;
  };

  WebMediaPlayerDelegate() {}

  // Subscribe or unsubscribe from observer callbacks respectively. A client
  // must use the delegate id returned by AddObserver() for all other calls.
  virtual int AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(int delegate_id) = 0;

  // The specified player started playing media.
  virtual void DidPlay(int delegate_id,
                       bool has_video,
                       bool has_audio,
                       bool is_remote,
                       media::MediaContentType media_content_type) = 0;

  // The specified player stopped playing media. This may be called at any time
  // with or without a DidPlay() having previously occurred. Calling this will
  // cause the delegate to be registered for idle suspension. I.e., after some
  // time elapses without a DidPlay(), OnSuspendRequested() will be issued.
  virtual void DidPause(int delegate_id, bool reached_end_of_stream) = 0;

  // The specified player was destroyed or suspended and will no longer accept
  // Observer::OnPlay() or Observer::OnPause() calls. This may be called
  // multiple times in row. Note: Clients must still call RemoveObserver() to
  // unsubscribe from callbacks.
  virtual void PlayerGone(int delegate_id) = 0;

  // Returns whether the render frame is currently hidden.
  virtual bool IsHidden() = 0;

  // Returns whether there's a video playing in background within the render
  // frame.
  virtual bool IsPlayingBackgroundVideo() = 0;

 protected:
  virtual ~WebMediaPlayerDelegate() {}
};

}  // namespace media

#endif  // MEDIA_BLINK_WEBMEDIAPLAYER_DELEGATE_H_
