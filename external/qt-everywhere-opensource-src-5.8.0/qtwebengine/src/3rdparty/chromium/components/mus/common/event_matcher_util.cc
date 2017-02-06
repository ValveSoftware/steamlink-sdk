// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/common/event_matcher_util.h"

namespace mus {

mojom::EventMatcherPtr CreateKeyMatcher(ui::mojom::KeyboardCode code,
                                        int flags) {
  mojom::EventMatcherPtr matcher(mojom::EventMatcher::New());
  matcher->type_matcher = mojom::EventTypeMatcher::New();
  matcher->flags_matcher = mojom::EventFlagsMatcher::New();
  matcher->ignore_flags_matcher = mojom::EventFlagsMatcher::New();
  // Ignoring these makes most accelerator scenarios more straight forward. Code
  // that needs to check them can override this setting.
  matcher->ignore_flags_matcher->flags = ui::mojom::kEventFlagCapsLockOn |
                                         ui::mojom::kEventFlagScrollLockOn |
                                         ui::mojom::kEventFlagNumLockOn;
  matcher->key_matcher = mojom::KeyEventMatcher::New();

  matcher->type_matcher->type = ui::mojom::EventType::KEY_PRESSED;
  matcher->flags_matcher->flags = flags;
  matcher->key_matcher->keyboard_code = code;
  return matcher;
}

}  // namespace mus
