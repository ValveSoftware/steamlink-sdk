// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/input_handler.h"

#include <stddef.h>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "cc/output/compositor_frame_metadata.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/input/synthetic_pinch_gesture_params.h"
#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"
#include "content/common/input/synthetic_tap_gesture_params.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/geometry/point.h"

namespace content {
namespace devtools {
namespace input {

namespace {

gfx::PointF CssPixelsToPointF(int x, int y, float page_scale_factor) {
  return gfx::PointF(x * page_scale_factor, y * page_scale_factor);
}

gfx::Vector2dF CssPixelsToVector2dF(int x, int y, float page_scale_factor) {
  return gfx::Vector2dF(x * page_scale_factor, y * page_scale_factor);
}

bool StringToGestureSourceType(const std::string& in,
                               SyntheticGestureParams::GestureSourceType& out) {
  if (in == kGestureSourceTypeDefault) {
    out = SyntheticGestureParams::GestureSourceType::DEFAULT_INPUT;
    return true;
  } else if (in == kGestureSourceTypeTouch) {
    out = SyntheticGestureParams::GestureSourceType::TOUCH_INPUT;
    return true;
  } else if (in == kGestureSourceTypeMouse) {
    out = SyntheticGestureParams::GestureSourceType::MOUSE_INPUT;
    return true;
  } else {
    return false;
  }
}

}

typedef DevToolsProtocolClient::Response Response;

namespace {

void SetEventModifiers(blink::WebInputEvent* event, const int* modifiers) {
  if (!modifiers)
    return;
  if (*modifiers & 1)
    event->modifiers |= blink::WebInputEvent::AltKey;
  if (*modifiers & 2)
    event->modifiers |= blink::WebInputEvent::ControlKey;
  if (*modifiers & 4)
    event->modifiers |= blink::WebInputEvent::MetaKey;
  if (*modifiers & 8)
    event->modifiers |= blink::WebInputEvent::ShiftKey;
}

void SetEventTimestamp(blink::WebInputEvent* event, const double* timestamp) {
  // Convert timestamp, in seconds since unix epoch, to an event timestamp
  // which is time ticks since platform start time.
  base::TimeTicks ticks = timestamp
                              ? base::TimeDelta::FromSecondsD(*timestamp) +
                                    base::TimeTicks::UnixEpoch()
                              : base::TimeTicks::Now();
  event->timeStampSeconds = (ticks - base::TimeTicks()).InSecondsF();
}

bool SetKeyboardEventText(blink::WebUChar* to, const std::string* from) {
  if (!from)
    return true;

  base::string16 text16 = base::UTF8ToUTF16(*from);
  if (text16.size() > blink::WebKeyboardEvent::textLengthCap)
    return false;

  for (size_t i = 0; i < text16.size(); ++i)
    to[i] = text16[i];
  return true;
}

bool SetMouseEventButton(blink::WebMouseEvent* event,
                         const std::string* button) {
  if (!button)
    return true;

  if (*button == dispatch_mouse_event::kButtonNone) {
    event->button = blink::WebMouseEvent::ButtonNone;
  } else if (*button == dispatch_mouse_event::kButtonLeft) {
    event->button = blink::WebMouseEvent::ButtonLeft;
    event->modifiers |= blink::WebInputEvent::LeftButtonDown;
  } else if (*button == dispatch_mouse_event::kButtonMiddle) {
    event->button = blink::WebMouseEvent::ButtonMiddle;
    event->modifiers |= blink::WebInputEvent::MiddleButtonDown;
  } else if (*button == dispatch_mouse_event::kButtonRight) {
    event->button = blink::WebMouseEvent::ButtonRight;
    event->modifiers |= blink::WebInputEvent::RightButtonDown;
  } else {
    return false;
  }
  return true;
}

bool SetMouseEventType(blink::WebMouseEvent* event, const std::string& type) {
  if (type == dispatch_mouse_event::kTypeMousePressed) {
    event->type = blink::WebInputEvent::MouseDown;
  } else if (type == dispatch_mouse_event::kTypeMouseReleased) {
    event->type = blink::WebInputEvent::MouseUp;
  } else if (type == dispatch_mouse_event::kTypeMouseMoved) {
    event->type = blink::WebInputEvent::MouseMove;
  } else {
    return false;
  }
  return true;
}

}  // namespace

InputHandler::InputHandler()
    : host_(NULL),
      page_scale_factor_(1.0),
      weak_factory_(this) {
}

InputHandler::~InputHandler() {
}

void InputHandler::SetRenderWidgetHost(RenderWidgetHostImpl* host) {
  host_ = host;
}

void InputHandler::SetClient(std::unique_ptr<Client> client) {
  client_.swap(client);
}

void InputHandler::OnSwapCompositorFrame(
    const cc::CompositorFrameMetadata& frame_metadata) {
  page_scale_factor_ = frame_metadata.page_scale_factor;
  scrollable_viewport_size_ = frame_metadata.scrollable_viewport_size;
}

Response InputHandler::DispatchKeyEvent(
    const std::string& type,
    const int* modifiers,
    const double* timestamp,
    const std::string* text,
    const std::string* unmodified_text,
    const std::string* key_identifier,
    const std::string* code,
    const std::string* key,
    const int* windows_virtual_key_code,
    const int* native_virtual_key_code,
    const bool* auto_repeat,
    const bool* is_keypad,
    const bool* is_system_key) {
  NativeWebKeyboardEvent event;

  if (type == dispatch_key_event::kTypeKeyDown) {
    event.type = blink::WebInputEvent::KeyDown;
  } else if (type == dispatch_key_event::kTypeKeyUp) {
    event.type = blink::WebInputEvent::KeyUp;
  } else if (type == dispatch_key_event::kTypeChar) {
    event.type = blink::WebInputEvent::Char;
  } else if (type == dispatch_key_event::kTypeRawKeyDown) {
    event.type = blink::WebInputEvent::RawKeyDown;
  } else {
    return Response::InvalidParams(
        base::StringPrintf("Unexpected event type '%s'", type.c_str()));
  }

  SetEventModifiers(&event, modifiers);
  SetEventTimestamp(&event, timestamp);
  if (!SetKeyboardEventText(event.text, text))
    return Response::InvalidParams("Invalid 'text' parameter");
  if (!SetKeyboardEventText(event.unmodifiedText, unmodified_text))
    return Response::InvalidParams("Invalid 'unmodifiedText' parameter");

  if (windows_virtual_key_code)
    event.windowsKeyCode = *windows_virtual_key_code;
  if (native_virtual_key_code)
    event.nativeKeyCode = *native_virtual_key_code;
  if (auto_repeat && *auto_repeat)
    event.modifiers |= blink::WebInputEvent::IsAutoRepeat;
  if (is_keypad && *is_keypad)
    event.modifiers |= blink::WebInputEvent::IsKeyPad;
  if (is_system_key)
    event.isSystemKey = *is_system_key;

  if (key_identifier) {
    if (key_identifier->size() >
        blink::WebKeyboardEvent::keyIdentifierLengthCap) {
      return Response::InvalidParams("Invalid 'keyIdentifier' parameter");
    }
    for (size_t i = 0; i < key_identifier->size(); ++i)
      event.keyIdentifier[i] = (*key_identifier)[i];
  } else if (event.type != blink::WebInputEvent::Char) {
    event.setKeyIdentifierFromWindowsKeyCode();
  }

  if (code) {
    event.domCode = static_cast<int>(
        ui::KeycodeConverter::CodeStringToDomCode(*code));
  }

  if (key) {
    event.domKey = static_cast<int>(
        ui::KeycodeConverter::KeyStringToDomKey(*key));
  }

  if (!host_)
    return Response::ServerError("Could not connect to view");

  host_->Focus();
  host_->ForwardKeyboardEvent(event);
  return Response::OK();
}

Response InputHandler::DispatchMouseEvent(
    const std::string& type,
    int x,
    int y,
    const int* modifiers,
    const double* timestamp,
    const std::string* button,
    const int* click_count) {
  blink::WebMouseEvent event;

  if (!SetMouseEventType(&event, type)) {
    return Response::InvalidParams(
        base::StringPrintf("Unexpected event type '%s'", type.c_str()));
  }
  SetEventModifiers(&event, modifiers);
  SetEventTimestamp(&event, timestamp);
  if (!SetMouseEventButton(&event, button))
    return Response::InvalidParams("Invalid mouse button");

  event.x = x * page_scale_factor_;
  event.y = y * page_scale_factor_;
  event.windowX = x * page_scale_factor_;
  event.windowY = y * page_scale_factor_;
  event.globalX = x * page_scale_factor_;
  event.globalY = y * page_scale_factor_;
  event.clickCount = click_count ? *click_count : 0;
  event.pointerType = blink::WebPointerProperties::PointerType::Mouse;

  if (!host_)
    return Response::ServerError("Could not connect to view");

  host_->Focus();
  host_->ForwardMouseEvent(event);
  return Response::OK();
}

Response InputHandler::EmulateTouchFromMouseEvent(const std::string& type,
                                                  int x,
                                                  int y,
                                                  double timestamp,
                                                  const std::string& button,
                                                  double* delta_x,
                                                  double* delta_y,
                                                  int* modifiers,
                                                  int* click_count) {
  blink::WebMouseWheelEvent wheel_event;
  blink::WebMouseEvent mouse_event;
  blink::WebMouseEvent* event = &mouse_event;

  if (type == emulate_touch_from_mouse_event::kTypeMouseWheel) {
    if (!delta_x || !delta_y) {
      return Response::InvalidParams(
          "'deltaX' and 'deltaY' are expected for mouseWheel event");
    }
    wheel_event.deltaX = static_cast<float>(*delta_x);
    wheel_event.deltaY = static_cast<float>(*delta_y);
    event = &wheel_event;
    event->type = blink::WebInputEvent::MouseWheel;
  } else if (!SetMouseEventType(event, type)) {
    return Response::InvalidParams(
        base::StringPrintf("Unexpected event type '%s'", type.c_str()));
  }

  SetEventModifiers(event, modifiers);
  SetEventTimestamp(event, &timestamp);
  if (!SetMouseEventButton(event, &button))
    return Response::InvalidParams("Invalid mouse button");

  event->x = x;
  event->y = y;
  event->windowX = x;
  event->windowY = y;
  event->globalX = x;
  event->globalY = y;
  event->clickCount = click_count ? *click_count : 0;
  event->pointerType = blink::WebPointerProperties::PointerType::Touch;

  if (!host_)
    return Response::ServerError("Could not connect to view");

  if (event->type == blink::WebInputEvent::MouseWheel)
    host_->ForwardWheelEvent(wheel_event);
  else
    host_->ForwardMouseEvent(mouse_event);
  return Response::OK();
}

Response InputHandler::SynthesizePinchGesture(
    DevToolsCommandId command_id,
    int x,
    int y,
    double scale_factor,
    const int* relative_speed,
    const std::string* gesture_source_type) {
  if (!host_)
    return Response::ServerError("Could not connect to view");

  SyntheticPinchGestureParams gesture_params;
  const int kDefaultRelativeSpeed = 800;

  gesture_params.scale_factor = scale_factor;
  gesture_params.anchor = CssPixelsToPointF(x, y, page_scale_factor_);
  gesture_params.relative_pointer_speed_in_pixels_s =
      relative_speed ? *relative_speed : kDefaultRelativeSpeed;

  if (!StringToGestureSourceType(
      gesture_source_type ? *gesture_source_type : kGestureSourceTypeDefault,
      gesture_params.gesture_source_type)) {
    return Response::InvalidParams("gestureSourceType");
  }

  host_->QueueSyntheticGesture(
      SyntheticGesture::Create(gesture_params),
      base::Bind(&InputHandler::SendSynthesizePinchGestureResponse,
                 weak_factory_.GetWeakPtr(), command_id));

  return Response::OK();
}

Response InputHandler::SynthesizeScrollGesture(
    DevToolsCommandId command_id,
    int x,
    int y,
    const int* x_distance,
    const int* y_distance,
    const int* x_overscroll,
    const int* y_overscroll,
    const bool* prevent_fling,
    const int* speed,
    const std::string* gesture_source_type,
    const int* repeat_count,
    const int* repeat_delay_ms,
    const std::string* interaction_marker_name) {
  if (!host_)
    return Response::ServerError("Could not connect to view");

  SyntheticSmoothScrollGestureParams gesture_params;
  const bool kDefaultPreventFling = true;
  const int kDefaultSpeed = 800;

  gesture_params.anchor = CssPixelsToPointF(x, y, page_scale_factor_);
  gesture_params.prevent_fling =
      prevent_fling ? *prevent_fling : kDefaultPreventFling;
  gesture_params.speed_in_pixels_s = speed ? *speed : kDefaultSpeed;

  if (x_distance || y_distance) {
    gesture_params.distances.push_back(
        CssPixelsToVector2dF(x_distance ? *x_distance : 0,
                             y_distance ? *y_distance : 0, page_scale_factor_));
  }

  if (x_overscroll || y_overscroll) {
    gesture_params.distances.push_back(CssPixelsToVector2dF(
        x_overscroll ? -*x_overscroll : 0, y_overscroll ? -*y_overscroll : 0,
        page_scale_factor_));
  }

  if (!StringToGestureSourceType(
      gesture_source_type ? *gesture_source_type : kGestureSourceTypeDefault,
      gesture_params.gesture_source_type)) {
    return Response::InvalidParams("gestureSourceType");
  }

  SynthesizeRepeatingScroll(
      gesture_params, repeat_count ? *repeat_count : 0,
      base::TimeDelta::FromMilliseconds(repeat_delay_ms ? *repeat_delay_ms
                                                        : 250),
      interaction_marker_name ? *interaction_marker_name : "", command_id);

  return Response::OK();
}

void InputHandler::SynthesizeRepeatingScroll(
    SyntheticSmoothScrollGestureParams gesture_params,
    int repeat_count,
    base::TimeDelta repeat_delay,
    std::string interaction_marker_name,
    DevToolsCommandId command_id) {
  if (!interaction_marker_name.empty()) {
    // TODO(alexclarke): Can we move this elsewhere? It doesn't really fit here.
    TRACE_EVENT_COPY_ASYNC_BEGIN0("benchmark", interaction_marker_name.c_str(),
                                  command_id.call_id);
  }

  host_->QueueSyntheticGesture(
      SyntheticGesture::Create(gesture_params),
      base::Bind(&InputHandler::OnScrollFinished, weak_factory_.GetWeakPtr(),
                 gesture_params, repeat_count, repeat_delay,
                 interaction_marker_name, command_id));
}

void InputHandler::OnScrollFinished(
    SyntheticSmoothScrollGestureParams gesture_params,
    int repeat_count,
    base::TimeDelta repeat_delay,
    std::string interaction_marker_name,
    DevToolsCommandId command_id,
    SyntheticGesture::Result result) {
  if (!interaction_marker_name.empty()) {
    TRACE_EVENT_COPY_ASYNC_END0("benchmark", interaction_marker_name.c_str(),
                                command_id.call_id);
  }

  if (repeat_count > 0) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&InputHandler::SynthesizeRepeatingScroll,
                   weak_factory_.GetWeakPtr(), gesture_params, repeat_count - 1,
                   repeat_delay, interaction_marker_name, command_id),
        repeat_delay);
  } else {
    SendSynthesizeScrollGestureResponse(command_id, result);
  }
}

Response InputHandler::SynthesizeTapGesture(
    DevToolsCommandId command_id,
    int x,
    int y,
    const int* duration,
    const int* tap_count,
    const std::string* gesture_source_type) {
  if (!host_)
    return Response::ServerError("Could not connect to view");

  SyntheticTapGestureParams gesture_params;
  const int kDefaultDuration = 50;
  const int kDefaultTapCount = 1;

  gesture_params.position = CssPixelsToPointF(x, y, page_scale_factor_);
  gesture_params.duration_ms = duration ? *duration : kDefaultDuration;

  if (!StringToGestureSourceType(
      gesture_source_type ? *gesture_source_type : kGestureSourceTypeDefault,
      gesture_params.gesture_source_type)) {
    return Response::InvalidParams("gestureSourceType");
  }

  if (!tap_count)
    tap_count = &kDefaultTapCount;

  for (int i = 0; i < *tap_count; i++) {
    // If we're doing more than one tap, don't send the response to the client
    // until we've completed the last tap.
    bool is_last_tap = i == *tap_count - 1;
    host_->QueueSyntheticGesture(
        SyntheticGesture::Create(gesture_params),
        base::Bind(&InputHandler::SendSynthesizeTapGestureResponse,
                   weak_factory_.GetWeakPtr(), command_id, is_last_tap));
  }

  return Response::OK();
}

void InputHandler::SendSynthesizePinchGestureResponse(
    DevToolsCommandId command_id,
    SyntheticGesture::Result result) {
  if (result == SyntheticGesture::Result::GESTURE_FINISHED) {
    client_->SendSynthesizePinchGestureResponse(
        command_id, SynthesizePinchGestureResponse::Create());
  } else {
    client_->SendError(command_id,
                       Response::InternalError(base::StringPrintf(
                           "Synthetic pinch failed, result was %d", result)));
  }
}

void InputHandler::SendSynthesizeScrollGestureResponse(
    DevToolsCommandId command_id,
    SyntheticGesture::Result result) {
  if (result == SyntheticGesture::Result::GESTURE_FINISHED) {
    client_->SendSynthesizeScrollGestureResponse(
        command_id, SynthesizeScrollGestureResponse::Create());
  } else {
    client_->SendError(command_id,
                       Response::InternalError(base::StringPrintf(
                           "Synthetic scroll failed, result was %d", result)));
  }
}

void InputHandler::SendSynthesizeTapGestureResponse(
    DevToolsCommandId command_id,
    bool send_success,
    SyntheticGesture::Result result) {
  if (result == SyntheticGesture::Result::GESTURE_FINISHED) {
    if (send_success) {
      client_->SendSynthesizeTapGestureResponse(
          command_id, SynthesizeTapGestureResponse::Create());
    }
  } else {
    client_->SendError(command_id,
                       Response::InternalError(base::StringPrintf(
                           "Synthetic tap failed, result was %d", result)));
  }
}

}  // namespace input
}  // namespace devtools
}  // namespace content
