// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ACCESSIBILITY_RENDERER_ACCESSIBILITY_H_
#define CONTENT_RENDERER_ACCESSIBILITY_RENDERER_ACCESSIBILITY_H_

#include "content/common/accessibility_messages.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/public/web/WebAXObject.h"

namespace blink {
class WebDocument;
};

namespace content {
class RenderViewImpl;

enum RendererAccessibilityType {
  // Turns on Blink accessibility and provides a full accessibility
  // implementation for when assistive technology is running.
  RendererAccessibilityTypeComplete,

  // Does not turn on Blink accessibility. Only sends a minimal accessible tree
  // to the browser whenever focus changes. This mode is currently used to
  // support opening the on-screen keyboard in response to touch events on
  // Windows 8 in Metro mode.
  RendererAccessibilityTypeFocusOnly
};

// The browser process implement native accessibility APIs, allowing
// assistive technology (e.g., screen readers, magnifiers) to access and
// control the web contents with high-level APIs. These APIs are also used
// by automation tools, and Windows 8 uses them to determine when the
// on-screen keyboard should be shown.
//
// An instance of this class (or rather, a subclass) belongs to RenderViewImpl.
// Accessibility is initialized based on the AccessibilityMode of
// RenderViewImpl; it lazily starts as Off or EditableTextOnly depending on
// the operating system, and switches to Complete if assistive technology is
// detected or a flag is set.
//
// A tree of accessible objects is built here and sent to the browser process;
// the browser process maintains this as a tree of platform-native
// accessible objects that can be used to respond to accessibility requests
// from other processes.
//
// This base class just contains common code and will not do anything by itself.
// The two subclasses are:
//
//   RendererAccessibilityComplete - turns on Blink accessibility and
//       provides a full accessibility implementation for when
//       assistive technology is running.
//
//   RendererAccessibilityFocusOnly - does not turn on Blink
//       accessibility. Only sends a minimal accessible tree to the
//       browser whenever focus changes. This mode is currently used
//       to support opening the on-screen keyboard in response to
//       touch events on Windows 8 in Metro mode.
//
class CONTENT_EXPORT RendererAccessibility : public RenderViewObserver {
 public:
  explicit RendererAccessibility(RenderViewImpl* render_view);
  virtual ~RendererAccessibility();

  // Called when an accessibility notification occurs in Blink.
  virtual void HandleWebAccessibilityEvent(
      const blink::WebAXObject& obj, blink::WebAXEvent event) = 0;

  // Gets the type of this RendererAccessibility object. Primarily intended for
  // testing.
  virtual RendererAccessibilityType GetType() = 0;

 protected:
  // Returns the main top-level document for this page, or NULL if there's
  // no view or frame.
  blink::WebDocument GetMainDocument();

  // The RenderViewImpl that owns us.
  RenderViewImpl* render_view_;

  DISALLOW_COPY_AND_ASSIGN(RendererAccessibility);
};

}  // namespace content

#endif  // CONTENT_RENDERER_ACCESSIBILITY_RENDERER_ACCESSIBILITY_H_
