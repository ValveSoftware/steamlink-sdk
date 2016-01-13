// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/renderer/media/mock_web_rtc_peer_connection_handler_client.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebString.h"

using testing::_;

namespace content {

MockWebRTCPeerConnectionHandlerClient::
MockWebRTCPeerConnectionHandlerClient()
    : candidate_mline_index_(-1) {
  ON_CALL(*this, didGenerateICECandidate(_)).WillByDefault(testing::Invoke(
      this,
      &MockWebRTCPeerConnectionHandlerClient::didGenerateICECandidateWorker));
  ON_CALL(*this, didAddRemoteStream(_)).WillByDefault(testing::Invoke(
      this,
      &MockWebRTCPeerConnectionHandlerClient::didAddRemoteStreamWorker));
  ON_CALL(*this, didRemoveRemoteStream(_)).WillByDefault(testing::Invoke(
      this,
      &MockWebRTCPeerConnectionHandlerClient::didRemoveRemoteStreamWorker));
}

MockWebRTCPeerConnectionHandlerClient::
~MockWebRTCPeerConnectionHandlerClient() {}

void MockWebRTCPeerConnectionHandlerClient::didGenerateICECandidateWorker(
    const blink::WebRTCICECandidate& candidate) {
  if (!candidate.isNull()) {
    candidate_sdp_ = base::UTF16ToUTF8(candidate.candidate());
    candidate_mline_index_ = candidate.sdpMLineIndex();
    candidate_mid_ = base::UTF16ToUTF8(candidate.sdpMid());
  } else {
    candidate_sdp_ = "";
    candidate_mline_index_ = -1;
    candidate_mid_ = "";
  }
}

void MockWebRTCPeerConnectionHandlerClient::didAddRemoteStreamWorker(
    const blink::WebMediaStream& stream_descriptor) {
  remote_steam_ = stream_descriptor;
}

void MockWebRTCPeerConnectionHandlerClient::didRemoveRemoteStreamWorker(
    const blink::WebMediaStream& stream_descriptor) {
  remote_steam_.reset();
}

}  // namespace content
