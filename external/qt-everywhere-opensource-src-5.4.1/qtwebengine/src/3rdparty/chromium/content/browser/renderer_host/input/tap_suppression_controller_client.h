// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_TAP_SUPPRESSION_CONTROLLER_CLIENT_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_TAP_SUPPRESSION_CONTROLLER_CLIENT_H_

namespace content {

// This class provides an interface for callbacks made by
// TapSuppressionController.
class TapSuppressionControllerClient {
 public:
  virtual ~TapSuppressionControllerClient() {}

  // Called whenever the deferred tap down (if saved) should be dropped totally.
  virtual void DropStashedTapDown() = 0;

  // Called whenever the deferred tap down (if saved) should be forwarded to the
  // renderer. The tap down should go back to normal path it was
  // on before being deferred.
  virtual void ForwardStashedTapDown() = 0;

 protected:
  TapSuppressionControllerClient() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TapSuppressionControllerClient);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_TAP_SUPPRESSION_CONTROLLER_CLIENT_H_
