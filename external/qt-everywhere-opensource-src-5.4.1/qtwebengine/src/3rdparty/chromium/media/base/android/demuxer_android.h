// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_DEMUXER_ANDROID_H_
#define MEDIA_BASE_ANDROID_DEMUXER_ANDROID_H_

#include "base/basictypes.h"
#include "base/time/time.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_export.h"

namespace media {

class DemuxerAndroidClient;
struct DemuxerConfigs;
struct DemuxerData;

// Defines a demuxer with asynchronous operations.
class MEDIA_EXPORT DemuxerAndroid {
 public:
  virtual ~DemuxerAndroid() {}

  // Initializes this demuxer with |client| as the callback handler.
  // Must be called prior to calling any other methods.
  virtual void Initialize(DemuxerAndroidClient* client) = 0;

  // Called to request additional data from the demuxer.
  virtual void RequestDemuxerData(media::DemuxerStream::Type type) = 0;

  // Called to request the demuxer to seek to a particular media time.
  // |is_browser_seek| is true if the renderer is not previously expecting this
  // seek and must coordinate with other regular seeks. Browser seek existence
  // should be hidden as much as possible from the renderer player and web apps.
  // TODO(wolenetz): Instead of doing browser seek, replay cached data since
  // last keyframe. See http://crbug.com/304234.
  virtual void RequestDemuxerSeek(const base::TimeDelta& time_to_seek,
                                  bool is_browser_seek) = 0;
};

// Defines the client callback interface.
class MEDIA_EXPORT DemuxerAndroidClient {
 public:
  // Called when the demuxer has initialized.
  virtual void OnDemuxerConfigsAvailable(const DemuxerConfigs& params) = 0;

  // Called in response to RequestDemuxerData().
  virtual void OnDemuxerDataAvailable(const DemuxerData& params) = 0;

  // Called in response to RequestDemuxerSeek().
  // If this is in response to a request with |is_browser_seek| set to true,
  // then |actual_browser_seek_time| may differ from the requested
  // |time_to_seek|, and reflects the actual time seeked to by the demuxer.
  // For regular demuxer seeks, |actual_browser_seek_time| is kNoTimestamp() and
  // should be ignored by browser player.
  virtual void OnDemuxerSeekDone(
      base::TimeDelta actual_browser_seek_time) = 0;

  // Called whenever the demuxer has detected a duration change.
  virtual void OnDemuxerDurationChanged(base::TimeDelta duration) = 0;

 protected:
  virtual ~DemuxerAndroidClient() {}
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_DEMUXER_ANDROID_H_
