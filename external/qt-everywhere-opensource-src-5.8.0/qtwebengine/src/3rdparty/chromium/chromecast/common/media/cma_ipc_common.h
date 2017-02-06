// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_COMMON_MEDIA_CMA_IPC_COMMON_H_
#define CHROMECAST_COMMON_MEDIA_CMA_IPC_COMMON_H_

namespace chromecast {
namespace media {

enum TrackId {
  kNoTrackId = -1,
  kAudioTrackId = 0,
  kVideoTrackId = 1,
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_COMMON_MEDIA_CMA_IPC_COMMON_H_

