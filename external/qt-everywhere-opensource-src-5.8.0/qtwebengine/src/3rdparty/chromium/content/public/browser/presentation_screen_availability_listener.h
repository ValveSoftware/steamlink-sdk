// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PRESENTATION_SCREEN_AVAILABILITY_LISTENER_H_
#define CONTENT_PUBLIC_BROWSER_PRESENTATION_SCREEN_AVAILABILITY_LISTENER_H_

#include <string>

#include "content/common/content_export.h"

namespace content {

// A listener interface used for receiving updates on screen availability
// associated with a presentation URL from an embedder.
// See also PresentationServiceDelegate.
class CONTENT_EXPORT PresentationScreenAvailabilityListener {
 public:
  virtual ~PresentationScreenAvailabilityListener() {}

  // Returns the screen availability URL associated with this listener.
  // Empty string means this object is listening for screen availability
  // for "1-UA" mode, i.e. offscreen tab rendering.
  virtual std::string GetAvailabilityUrl() const = 0;

  // Called when screen availability for the associated Presentation URL has
  // changed to |available|.
  virtual void OnScreenAvailabilityChanged(bool available) = 0;

  // Callend when screen availability monitoring is not supported by the
  // by the implementation because of system limitations like running low on
  // battery or having resource constraints.
  virtual void OnScreenAvailabilityNotSupported() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PRESENTATION_SCREEN_AVAILABILITY_LISTENER_H_
