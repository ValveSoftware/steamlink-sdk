// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebEventListenerProperties_h
#define WebEventListenerProperties_h

#include "WebCommon.h"

namespace blink {

enum class WebEventListenerClass {
  TouchStartOrMove,  // This value includes "touchstart", "touchmove" and
                     // "pointer" events.
  MouseWheel,        // This value includes "wheel" and "mousewheel" events.
  TouchEndOrCancel,  // This value includes "touchend", "touchcancel" events.
};

// Indicates the variety of event listener types for a given
// WebEventListenerClass.
enum class WebEventListenerProperties {
  Nothing,             // This should be "None"; but None #defined in X11's X.h
  Passive,             // This indicates solely passive listeners.
  Blocking,            // This indicates solely blocking listeners.
  BlockingAndPassive,  // This indicates >= 1 blocking listener and >= 1 passive
                       // listeners.
};

}  // namespace blink

#endif
