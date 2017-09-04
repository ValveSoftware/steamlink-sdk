// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DeviceOrientationAbsoluteController_h
#define DeviceOrientationAbsoluteController_h

#include "core/dom/Document.h"
#include "modules/ModulesExport.h"
#include "modules/device_orientation/DeviceOrientationController.h"

namespace blink {

class MODULES_EXPORT DeviceOrientationAbsoluteController final
    : public DeviceOrientationController {
 public:
  ~DeviceOrientationAbsoluteController() override;

  static const char* supplementName();
  static DeviceOrientationAbsoluteController& from(Document&);

  // Inherited from DeviceSingleWindowEventController.
  void didAddEventListener(LocalDOMWindow*,
                           const AtomicString& eventType) override;

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit DeviceOrientationAbsoluteController(Document&);

  // Inherited from DeviceOrientationController.
  DeviceOrientationDispatcher& dispatcherInstance() const override;
  const AtomicString& eventTypeName() const override;
};

}  // namespace blink

#endif  // DeviceOrientationAbsoluteController_h
