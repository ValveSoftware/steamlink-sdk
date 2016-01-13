// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ACCESSIBILITY_BLINK_AX_ENUM_CONVERSION_H_
#define CONTENT_RENDERER_ACCESSIBILITY_BLINK_AX_ENUM_CONVERSION_H_

#include "third_party/WebKit/public/web/WebAXObject.h"
#include "ui/accessibility/ax_enums.h"

namespace content {

// Convert a Blink WebAXRole to an AXRole defined in ui/accessibility.
ui::AXRole AXRoleFromBlink(blink::WebAXRole role);

// Convert a Blink WebAXEvent to an AXEvent defined in ui/accessibility.
ui::AXEvent AXEventFromBlink(blink::WebAXEvent event);

// Provides a conversion between the WebAXObject state
// accessors and a state bitmask stored in an AXNodeData.
// (Note that some rare states are sent as boolean attributes
// in AXNodeData instead.)
uint32 AXStateFromBlink(const blink::WebAXObject& o);

// Convert a Blink WebAXTextDirection to an AXTextDirection defined in
// ui/accessibility.
ui::AXTextDirection AXTextDirectionFromBlink(
    blink::WebAXTextDirection text_direction);

}  // namespace content

#endif  // CONTENT_RENDERER_ACCESSIBILITY_BLINK_AX_ENUM_CONVERSION_H_
