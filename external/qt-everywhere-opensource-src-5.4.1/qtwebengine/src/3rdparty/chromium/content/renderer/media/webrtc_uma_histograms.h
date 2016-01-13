// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_UMA_HISTOGRAMS_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_UMA_HISTOGRAMS_H_

#include "base/memory/singleton.h"
#include "base/threading/non_thread_safe.h"
#include "content/common/content_export.h"

namespace content {

// Helper enum used for histogramming calls to WebRTC APIs from JavaScript.
enum JavaScriptAPIName {
  WEBKIT_GET_USER_MEDIA,
  WEBKIT_PEER_CONNECTION,
  WEBKIT_DEPRECATED_PEER_CONNECTION,
  WEBKIT_RTC_PEER_CONNECTION,
  WEBKIT_GET_MEDIA_DEVICES,
  INVALID_NAME
};

// Helper method used to collect information about the number of times
// different WebRTC APIs are called from JavaScript.
//
// This contributes to two histograms; the former is a raw count of
// the number of times the APIs are called, and be viewed at
// chrome://histograms/WebRTC.webkitApiCount.
//
// The latter is a count of the number of times the APIs are called
// that gets incremented only once per "session" as established by the
// PerSessionWebRTCAPIMetrics singleton below. It can be viewed at
// chrome://histograms/WebRTC.webkitApiCountPerSession.
void UpdateWebRTCMethodCount(JavaScriptAPIName api_name);

// A singleton that keeps track of the number of MediaStreams being
// sent over PeerConnections. It uses the transition to zero such
// streams to demarcate the start of a new "session". Note that this
// is a rough approximation of sessions, as you could conceivably have
// multiple tabs using this renderer process, and each of them using
// PeerConnections.
//
// The UpdateWebRTCMethodCount function above uses this class to log a
// metric at most once per session.
class CONTENT_EXPORT PerSessionWebRTCAPIMetrics : public base::NonThreadSafe {
 public:
  virtual ~PerSessionWebRTCAPIMetrics();

  static PerSessionWebRTCAPIMetrics* GetInstance();

  // Increment/decrement the number of streams being sent or received
  // over any current PeerConnection.
  void IncrementStreamCounter();
  void DecrementStreamCounter();

 protected:
  friend struct DefaultSingletonTraits<PerSessionWebRTCAPIMetrics>;
  friend void UpdateWebRTCMethodCount(JavaScriptAPIName);

  // Protected so that unit tests can test without this being a
  // singleton.
  PerSessionWebRTCAPIMetrics();

  // Overridable by unit tests.
  virtual void LogUsage(JavaScriptAPIName api_name);

  // Called by UpdateWebRTCMethodCount above. Protected rather than
  // private so that unit tests can call it.
  void LogUsageOnlyOnce(JavaScriptAPIName api_name);

 private:
  void ResetUsage();

  int num_streams_;
  bool has_used_api_[INVALID_NAME];

  DISALLOW_COPY_AND_ASSIGN(PerSessionWebRTCAPIMetrics);
};

} //  namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_UMA_HISTOGRAMS_H_
