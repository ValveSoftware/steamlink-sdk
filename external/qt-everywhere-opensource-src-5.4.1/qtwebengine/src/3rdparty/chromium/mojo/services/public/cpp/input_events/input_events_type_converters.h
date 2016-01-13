// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_INPUT_EVENTS_INPUT_EVENTS_TYPE_CONVERTERS_H_
#define MOJO_SERVICES_PUBLIC_CPP_INPUT_EVENTS_INPUT_EVENTS_TYPE_CONVERTERS_H_

#include "base/memory/scoped_ptr.h"
#include "mojo/services/public/cpp/input_events/mojo_input_events_export.h"
#include "mojo/services/public/interfaces/input_events/input_events.mojom.h"
#include "ui/events/event.h"

namespace mojo {

template<>
class MOJO_INPUT_EVENTS_EXPORT TypeConverter<EventPtr, ui::Event> {
 public:
  static EventPtr ConvertFrom(const ui::Event& input);
};

template<>
class MOJO_INPUT_EVENTS_EXPORT TypeConverter<EventPtr,
                                             scoped_ptr<ui::Event> > {
 public:
  static scoped_ptr<ui::Event> ConvertTo(const EventPtr& input);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_INPUT_EVENTS_INPUT_EVENTS_TYPE_CONVERTERS_H_
