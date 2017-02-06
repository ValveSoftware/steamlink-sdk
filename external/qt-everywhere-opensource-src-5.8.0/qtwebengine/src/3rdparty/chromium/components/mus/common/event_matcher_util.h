// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_COMMON_EVENT_MATCHER_UTIL_H_
#define COMPONENTS_MUS_COMMON_EVENT_MATCHER_UTIL_H_

#include "components/mus/common/mus_common_export.h"
#include "components/mus/public/interfaces/event_matcher.mojom.h"
#include "ui/events/mojo/event_constants.mojom.h"
#include "ui/events/mojo/keyboard_codes.mojom.h"

namespace mus {

// |flags| is a bitfield of kEventFlag* and kMouseEventFlag* values in
// input_event_constants.mojom.
mojom::EventMatcherPtr MUS_COMMON_EXPORT
CreateKeyMatcher(ui::mojom::KeyboardCode code, int flags);

}  // namespace mus

#endif  // COMPONENTS_MUS_COMMON_EVENT_MATCHER_UTIL_H_
