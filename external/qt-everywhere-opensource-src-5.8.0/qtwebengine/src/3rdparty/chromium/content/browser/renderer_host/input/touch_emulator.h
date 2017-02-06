// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_EMULATOR_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_EMULATOR_H_

#include <memory>

#include "base/macros.h"
#include "content/browser/renderer_host/input/touch_emulator_client.h"
#include "content/common/cursors/webcursor.h"
#include "content/common/input/input_event_ack_state.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/gesture_detection/filtered_gesture_provider.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"
#include "ui/gfx/geometry/size_f.h"

namespace content {

// Emulates touch input with mouse and keyboard.
class CONTENT_EXPORT TouchEmulator : public ui::GestureProviderClient {
 public:
  TouchEmulator(TouchEmulatorClient* client, float device_scale_factor);
  ~TouchEmulator() override;

  void Enable(ui::GestureProviderConfigType config_type);
  void Disable();

  // See GestureProvider::SetDoubleTapSupportForPageEnabled.
  void SetDoubleTapSupportForPageEnabled(bool enabled);

  // Note that TouchEmulator should always listen to touch events and their acks
  // (even in disabled state) to track native stream presence.
  bool enabled() const { return !!gesture_provider_; }

  // Returns |true| if the event was consumed. Consumed event should not
  // propagate any further.
  // TODO(dgozman): maybe pass latency info together with events.
  bool HandleMouseEvent(const blink::WebMouseEvent& event);
  bool HandleMouseWheelEvent(const blink::WebMouseWheelEvent& event);
  bool HandleKeyboardEvent(const blink::WebKeyboardEvent& event);
  bool HandleTouchEvent(const blink::WebTouchEvent& event);

  // Returns |true| if the event ack was consumed. Consumed ack should not
  // propagate any further.
  bool HandleTouchEventAck(const blink::WebTouchEvent& event,
                           InputEventAckState ack_result);

  // Cancel any touches, for example, when focus is lost.
  void CancelTouch();

 private:
  // ui::GestureProviderClient implementation.
  void OnGestureEvent(const ui::GestureEventData& gesture) override;

  // Returns cursor size in DIP.
  gfx::SizeF InitCursorFromResource(
      WebCursor* cursor, float scale, int resource_id);
  void ResetState();
  void UpdateCursor();
  bool UpdateShiftPressed(bool shift_pressed);

  // Whether we should convert scrolls into pinches.
  bool InPinchGestureMode() const;

  void FillTouchEventAndPoint(const blink::WebMouseEvent& mouse_event);
  void FillPinchEvent(const blink::WebInputEvent& event);

  // The following methods generate and pass gesture events to the renderer.
  void PinchBegin(const blink::WebGestureEvent& event);
  void PinchUpdate(const blink::WebGestureEvent& event);
  void PinchEnd(const blink::WebGestureEvent& event);
  void ScrollEnd(const blink::WebGestureEvent& event);

  // Offers the emulated event to |gesture_provider_|, conditionally forwarding
  // it to the client if appropriate.
  void HandleEmulatedTouchEvent(blink::WebTouchEvent event);

  TouchEmulatorClient* const client_;

  // Emulator is enabled iff gesture provider is created.
  // Disabled emulator does only process touch acks left from previous
  // emulation. It does not intercept any events.
  std::unique_ptr<ui::FilteredGestureProvider> gesture_provider_;
  ui::GestureProviderConfigType gesture_provider_config_type_;
  bool double_tap_enabled_;

  // While emulation is on, default cursor is touch. Pressing shift changes
  // cursor to the pinch one.
  WebCursor pointer_cursor_;
  WebCursor touch_cursor_;
  WebCursor pinch_cursor_;
  gfx::SizeF cursor_size_;

  // These are used to drop extra mouse move events coming too quickly, so
  // we don't handle too much touches in gesture provider.
  bool last_mouse_event_was_move_;
  double last_mouse_move_timestamp_;

  bool mouse_pressed_;
  bool shift_pressed_;

  blink::WebTouchEvent touch_event_;
  int emulated_stream_active_sequence_count_;
  int native_stream_active_sequence_count_;

  // Whether we should suppress next fling cancel. This may happen when we
  // did not send fling start in pinch mode.
  bool suppress_next_fling_cancel_;

  blink::WebGestureEvent pinch_event_;
  // Point which does not move while pinch-zooming.
  gfx::Point pinch_anchor_;
  // The cumulative scale change from the start of pinch gesture.
  float pinch_scale_;
  bool pinch_gesture_active_;

  DISALLOW_COPY_AND_ASSIGN(TouchEmulator);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_EMULATOR_H_
