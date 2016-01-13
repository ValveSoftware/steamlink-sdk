// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_NPAPI_PLUGIN_WEB_EVENT_CONVERTER_MAC_H_
#define CONTENT_CHILD_NPAPI_PLUGIN_WEB_EVENT_CONVERTER_MAC_H_

#include "third_party/npapi/bindings/npapi.h"

namespace blink {
class WebInputEvent;
class WebKeyboardEvent;
class WebMouseEvent;
class WebMouseWheelEvent;
}

namespace content {

// Utility class to translating WebInputEvent structs to equivalent structures
// suitable for sending to Mac plugins (via NPP_HandleEvent).
class PluginWebEventConverter {
 public:
  PluginWebEventConverter();
  virtual ~PluginWebEventConverter();

  // Initializes a converter for the given web event. Returns false if the event
  // could not be converted.
  bool InitWithEvent(const blink::WebInputEvent& web_event);

  // Returns a pointer to a plugin event--suitable for passing to
  // NPP_HandleEvent--corresponding to the the web event this converter was
  // created with. The pointer is valid only as long as this object is.
  // Returns NULL iff InitWithEvent returned false.
  NPCocoaEvent* plugin_event() { return &cocoa_event_; }

 private:
  // Stores a converted plugin representation of the given web event, suitable
  // for returning from plugin_event.
  // Returns true if the event was successfully converted.
  bool ConvertKeyboardEvent(const blink::WebKeyboardEvent& web_event);
  bool ConvertMouseEvent(const blink::WebMouseEvent& web_event);
  bool ConvertMouseWheelEvent(const blink::WebMouseWheelEvent& web_event);

  // Returns the Cocoa translation of web_event's modifiers.
  static NSUInteger CocoaModifiers(const blink::WebInputEvent& web_event);

  NPCocoaEvent cocoa_event_;

  DISALLOW_COPY_AND_ASSIGN(PluginWebEventConverter);
};

}  // namespace content

#endif  // CONTENT_CHILD_NPAPI_PLUGIN_WEB_EVENT_CONVERTER_MAC_H_
