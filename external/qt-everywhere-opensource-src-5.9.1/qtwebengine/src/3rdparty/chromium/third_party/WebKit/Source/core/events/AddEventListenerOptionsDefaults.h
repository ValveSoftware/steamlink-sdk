// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AddEventListenerOptionsDefaults_h
#define AddEventListenerOptionsDefaults_h

namespace blink {

// Defines the default for 'passive' field used in the AddEventListenerOptions
// interface when javascript calls addEventListener.
// |False| is the default specified in
// https://dom.spec.whatwg.org/#dictdef-addeventlisteneroptions. However
// specifying a different default value is useful in demonstrating the
// power of passive event listeners.
enum class PassiveListenerDefault {
  False,        // Default of false.
  True,         // Default of true.
  ForceAllTrue  // Force all values to be true even when specified.
};

}  // namespace blink

#endif  // AddEventListenerOptionsDefaults_h
