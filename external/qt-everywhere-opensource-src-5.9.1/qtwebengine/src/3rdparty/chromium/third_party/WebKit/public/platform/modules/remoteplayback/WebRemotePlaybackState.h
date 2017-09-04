// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebRemotePlaybackState_h
#define WebRemotePlaybackState_h

namespace blink {

enum class WebRemotePlaybackState {
  Connecting = 0,
  Connected,
  Disconnected,
};

}  // namespace blink

#endif  // WebRemotePlaybackState_h
