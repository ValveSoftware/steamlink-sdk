// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CONVERTERS_BLINK_BLINK_INPUT_EVENTS_TYPE_CONVERTERS_H_
#define MOJO_CONVERTERS_BLINK_BLINK_INPUT_EVENTS_TYPE_CONVERTERS_H_

#include <memory>

#include "mojo/converters/blink/mojo_blink_export.h"
#include "mojo/public/cpp/bindings/type_converter.h"

namespace blink {
class WebInputEvent;
}

namespace ui {
class Event;
}

namespace mojo {

template <>
struct MOJO_BLINK_EXPORT
    TypeConverter<std::unique_ptr<blink::WebInputEvent>, ui::Event> {
  static std::unique_ptr<blink::WebInputEvent> Convert(const ui::Event& input);
};

}  // namespace mojo

#endif  // MOJO_CONVERTERS_BLINK_BLINK_INPUT_EVENTS_TYPE_CONVERTERS_H_
