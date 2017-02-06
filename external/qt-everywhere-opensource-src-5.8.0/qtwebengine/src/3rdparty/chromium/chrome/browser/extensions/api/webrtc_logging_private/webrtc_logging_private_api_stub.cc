// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Stub implementation used when WebRTC is not enabled.

#include "chrome/browser/extensions/api/webrtc_logging_private/webrtc_logging_private_api.h"

namespace extensions {

namespace {

const char kErrorNotSupported[] = "Not supported";

}  // namespace

bool WebrtcLoggingPrivateSetMetaDataFunction::RunAsync() {
  SetError(kErrorNotSupported);
  SendResponse(false);
  return false;
}

bool WebrtcLoggingPrivateStartFunction::RunAsync() {
  SetError(kErrorNotSupported);
  SendResponse(false);
  return false;
}

bool WebrtcLoggingPrivateSetUploadOnRenderCloseFunction::RunAsync() {
  SetError(kErrorNotSupported);
  SendResponse(false);
  return false;
}

bool WebrtcLoggingPrivateStopFunction::RunAsync() {
  SetError(kErrorNotSupported);
  SendResponse(false);
  return false;
}

bool WebrtcLoggingPrivateStoreFunction::RunAsync() {
  SetError(kErrorNotSupported);
  SendResponse(false);
  return false;
}

bool WebrtcLoggingPrivateUploadStoredFunction::RunAsync() {
  SetError(kErrorNotSupported);
  SendResponse(false);
  return false;
}

bool WebrtcLoggingPrivateUploadFunction::RunAsync() {
  SetError(kErrorNotSupported);
  SendResponse(false);
  return false;
}

bool WebrtcLoggingPrivateDiscardFunction::RunAsync() {
  SetError(kErrorNotSupported);
  SendResponse(false);
  return false;
}

bool WebrtcLoggingPrivateStartRtpDumpFunction::RunAsync() {
  SetError(kErrorNotSupported);
  SendResponse(false);
  return false;
}

bool WebrtcLoggingPrivateStopRtpDumpFunction::RunAsync() {
  SetError(kErrorNotSupported);
  SendResponse(false);
  return false;
}

bool WebrtcLoggingPrivateStartAudioDebugRecordingsFunction::RunAsync() {
  SetError(kErrorNotSupported);
  SendResponse(false);
  return false;
}

bool WebrtcLoggingPrivateStopAudioDebugRecordingsFunction::RunAsync() {
  SetError(kErrorNotSupported);
  SendResponse(false);
  return false;
}

bool WebrtcLoggingPrivateStartWebRtcEventLoggingFunction::RunAsync() {
  SetError(kErrorNotSupported);
  SendResponse(false);
  return false;
}

bool WebrtcLoggingPrivateStopWebRtcEventLoggingFunction::RunAsync() {
  SetError(kErrorNotSupported);
  SendResponse(false);
  return false;
}

}  // namespace extensions
