// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebRemotePlaybackClient_h
#define WebRemotePlaybackClient_h

namespace blink {

enum class WebRemotePlaybackAvailability;
enum class WebRemotePlaybackState;

// The interface between the HTMLMediaElement and its
// HTMLMediaElementRemotePlayback supplement.
class WebRemotePlaybackClient {
 public:
  virtual ~WebRemotePlaybackClient() = default;

  // Notifies the client that the media element state has changed.
  virtual void stateChanged(WebRemotePlaybackState) = 0;

  // Notifies the client of the remote playback device availability change.
  virtual void availabilityChanged(WebRemotePlaybackAvailability) = 0;

  // Notifies the client that the user cancelled the prompt shown via the API.
  virtual void promptCancelled() = 0;

  // Returns if the remote playback available for this media element.
  virtual bool remotePlaybackAvailable() const = 0;
};

}  // namespace blink

#endif  // WebRemotePlaybackState_h
