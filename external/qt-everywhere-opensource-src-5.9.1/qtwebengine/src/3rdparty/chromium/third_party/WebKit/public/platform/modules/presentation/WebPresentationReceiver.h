// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPresentationReceiver_h
#define WebPresentationReceiver_h

#include "public/platform/WebCommon.h"

namespace blink {

class WebPresentationConnectionClient;

// The delegate Blink provides to WebPresentationReceiverClient in order to get
// updates.
class BLINK_PLATFORM_EXPORT WebPresentationReceiver {
 public:
  virtual ~WebPresentationReceiver() = default;

  // Called when receiver page gets an incoming connection.
  virtual void onReceiverConnectionAvailable(
      WebPresentationConnectionClient*) = 0;
};

}  // namespace blink

#endif  // WebPresentationReceiver_h
