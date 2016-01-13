// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_VIBRATION_PROVIDER_H_
#define CONTENT_PUBLIC_BROWSER_VIBRATION_PROVIDER_H_

namespace content {

class VibrationProvider {
 public:
  // Device should start vibrating for N milliseconds.
  virtual void Vibrate(int64 milliseconds) = 0;

  // Cancels vibration of the device.
  virtual void CancelVibration() = 0;

  virtual ~VibrationProvider() {}
};

}  // namespace content

#endif // CONTENT_PUBLIC_BROWSER_VIBRATION_PROVIDER_H_
