// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_COMMON_EVENT_MATCHER_UTIL_H_
#define SERVICES_UI_COMMON_EVENT_MATCHER_UTIL_H_

#include "services/ui/public/interfaces/event_matcher.mojom.h"
#include "ui/events/mojo/event_constants.mojom.h"
#include "ui/events/mojo/keyboard_codes.mojom.h"

namespace ui {

// |flags| is a bitfield of kEventFlag* and kMouseEventFlag* values in
// input_event_constants.mojom.
mojom::EventMatcherPtr CreateKeyMatcher(ui::mojom::KeyboardCode code,
                                        int flags);

}  // namespace ui

#endif  // SERVICES_UI_COMMON_EVENT_MATCHER_UTIL_H_
