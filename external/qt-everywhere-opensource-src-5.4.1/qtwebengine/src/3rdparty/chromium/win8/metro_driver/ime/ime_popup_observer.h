// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIN8_METRO_DRIVER_IME_IME_POPUP_OBSERVER_H_
#define WIN8_METRO_DRIVER_IME_IME_POPUP_OBSERVER_H_

namespace metro_driver {

// An observer interface implemented by objects that want to be informed when
// an IME shows or hides its popup window.
class ImePopupObserver {
 public:
  enum EventType {
    kPopupShown,
    kPopupHidden,
    kPopupUpdated,
  };
  virtual ~ImePopupObserver() {}

  // Called whenever an IME's popup window is changed.
  virtual void OnImePopupChanged(EventType type) = 0;
};

}  // namespace metro_driver

#endif  // WIN8_METRO_DRIVER_IME_IME_POPUP_OBSERVER_H_
