// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webusb/NavigatorUSB.h"

#include "core/frame/Navigator.h"
#include "modules/webusb/USB.h"

namespace blink {

NavigatorUSB& NavigatorUSB::from(Navigator& navigator) {
  NavigatorUSB* supplement = static_cast<NavigatorUSB*>(
      Supplement<Navigator>::from(navigator, supplementName()));
  if (!supplement) {
    supplement = new NavigatorUSB(navigator);
    provideTo(navigator, supplementName(), supplement);
  }
  return *supplement;
}

USB* NavigatorUSB::usb(Navigator& navigator) {
  return NavigatorUSB::from(navigator).usb();
}

USB* NavigatorUSB::usb() {
  return m_usb;
}

DEFINE_TRACE(NavigatorUSB) {
  visitor->trace(m_usb);
  Supplement<Navigator>::trace(visitor);
}

NavigatorUSB::NavigatorUSB(Navigator& navigator) {
  if (navigator.frame())
    m_usb = USB::create(*navigator.frame());
}

const char* NavigatorUSB::supplementName() {
  return "NavigatorUSB";
}

}  // namespace blink
