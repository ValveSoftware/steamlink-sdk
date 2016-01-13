// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/event_converter_evdev.h"

#include "ui/events/event.h"
#include "ui/ozone/public/event_factory_ozone.h"

namespace ui {

EventConverterEvdev::EventConverterEvdev() {}

EventConverterEvdev::EventConverterEvdev(const EventDispatchCallback& callback)
    : dispatch_callback_(callback) {}

EventConverterEvdev::~EventConverterEvdev() {}

void EventConverterEvdev::DispatchEventToCallback(ui::Event* event) {
  dispatch_callback_.Run(event);
}

}  // namespace ui
