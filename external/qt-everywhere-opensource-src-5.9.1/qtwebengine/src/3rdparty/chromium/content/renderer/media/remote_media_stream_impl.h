// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_REMOTE_MEDIA_STREAM_IMPL_H_
#define CONTENT_RENDERER_MEDIA_REMOTE_MEDIA_STREAM_IMPL_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/webrtc/api/mediastreaminterface.h"

namespace content {

class RemoteAudioTrackAdapter;
class RemoteVideoTrackAdapter;

// RemoteMediaStreamImpl serves as a container and glue between remote webrtc
// MediaStreams and WebKit MediaStreams. For each remote MediaStream received
// on a PeerConnection a RemoteMediaStreamImpl instance is created and
// owned by RtcPeerConnection.
class CONTENT_EXPORT RemoteMediaStreamImpl {
 public:
  RemoteMediaStreamImpl(
      const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
      webrtc::MediaStreamInterface* webrtc_stream);
  ~RemoteMediaStreamImpl();

  const blink::WebMediaStream& webkit_stream() { return webkit_stream_; }
  const scoped_refptr<webrtc::MediaStreamInterface>& webrtc_stream() {
    return observer_->stream();
  }

 private:
  typedef std::vector<scoped_refptr<RemoteAudioTrackAdapter>>
      RemoteAudioTrackAdapters;
  typedef std::vector<scoped_refptr<RemoteVideoTrackAdapter>>
      RemoteVideoTrackAdapters;

  void InitializeOnMainThread(const std::string& label);

  class Observer
      : NON_EXPORTED_BASE(public webrtc::ObserverInterface),
        public base::RefCountedThreadSafe<Observer> {
   public:
    Observer(const base::WeakPtr<RemoteMediaStreamImpl>& media_stream,
             const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
             webrtc::MediaStreamInterface* webrtc_stream);

    const scoped_refptr<webrtc::MediaStreamInterface>& stream() const {
      return webrtc_stream_;
    }

    const scoped_refptr<base::SingleThreadTaskRunner>& main_thread() const {
      return main_thread_;
    }

    void InitializeOnMainThread(const std::string& label);

    // Uninitializes the observer, unregisteres from receiving notifications
    // and releases the webrtc stream.
    // Note: Must be called from the main thread before releasing the main
    // reference.
    void Unregister();

   private:
    friend class base::RefCountedThreadSafe<Observer>;
    ~Observer() override;

    // webrtc::ObserverInterface implementation.
    void OnChanged() override;

    void OnChangedOnMainThread(
        std::unique_ptr<RemoteAudioTrackAdapters> audio_tracks,
        std::unique_ptr<RemoteVideoTrackAdapters> video_tracks);

    base::WeakPtr<RemoteMediaStreamImpl> media_stream_;
    const scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
    scoped_refptr<webrtc::MediaStreamInterface> webrtc_stream_;
  };

  void OnChanged(std::unique_ptr<RemoteAudioTrackAdapters> audio_tracks,
                 std::unique_ptr<RemoteVideoTrackAdapters> video_tracks);

  const scoped_refptr<base::SingleThreadTaskRunner> signaling_thread_;
  scoped_refptr<Observer> observer_;

  RemoteVideoTrackAdapters video_track_observers_;
  RemoteAudioTrackAdapters audio_track_observers_;
  blink::WebMediaStream webkit_stream_;

  base::WeakPtrFactory<RemoteMediaStreamImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(RemoteMediaStreamImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_REMOTE_MEDIA_STREAM_IMPL_H_
