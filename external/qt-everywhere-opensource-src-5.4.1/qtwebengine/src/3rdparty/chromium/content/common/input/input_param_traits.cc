// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/input_param_traits.h"

#include "content/common/content_param_traits.h"
#include "content/common/input/synthetic_pinch_gesture_params.h"
#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"
#include "content/common/input/web_input_event_traits.h"
#include "content/common/input_messages.h"

namespace IPC {
namespace {
template<typename GestureType>
scoped_ptr<content::SyntheticGestureParams> ReadGestureParams(
    const Message* m,
    PickleIterator* iter)
{
  scoped_ptr<GestureType> gesture_params(new GestureType);
  if (!ReadParam(m, iter, gesture_params.get()))
    return scoped_ptr<content::SyntheticGestureParams>();

  return gesture_params.template PassAs<content::SyntheticGestureParams>();
}
}  // namespace

void ParamTraits<content::ScopedWebInputEvent>::Write(Message* m,
                                                      const param_type& p) {
  bool valid_web_event = !!p;
  WriteParam(m, valid_web_event);
  if (valid_web_event)
    WriteParam(m, static_cast<WebInputEventPointer>(p.get()));
}

bool ParamTraits<content::ScopedWebInputEvent>::Read(const Message* m,
                                                     PickleIterator* iter,
                                                     param_type* p) {
  bool valid_web_event = false;
  WebInputEventPointer web_event_pointer = NULL;
  if (!ReadParam(m, iter, &valid_web_event) ||
      !valid_web_event ||
      !ReadParam(m, iter, &web_event_pointer) ||
      !web_event_pointer)
    return false;

  (*p) = content::WebInputEventTraits::Clone(*web_event_pointer);
  return true;
}

void ParamTraits<content::ScopedWebInputEvent>::Log(const param_type& p,
                                                    std::string* l) {
  LogParam(static_cast<WebInputEventPointer>(p.get()), l);
}

void ParamTraits<content::SyntheticGesturePacket>::Write(Message* m,
                                                         const param_type& p) {
  DCHECK(p.gesture_params());
  WriteParam(m, p.gesture_params()->GetGestureType());
  switch (p.gesture_params()->GetGestureType()) {
    case content::SyntheticGestureParams::SMOOTH_SCROLL_GESTURE:
      WriteParam(m, *content::SyntheticSmoothScrollGestureParams::Cast(
          p.gesture_params()));
      break;
    case content::SyntheticGestureParams::PINCH_GESTURE:
      WriteParam(m, *content::SyntheticPinchGestureParams::Cast(
          p.gesture_params()));
      break;
    case content::SyntheticGestureParams::TAP_GESTURE:
      WriteParam(m, *content::SyntheticTapGestureParams::Cast(
          p.gesture_params()));
      break;
  }
}

bool ParamTraits<content::SyntheticGesturePacket>::Read(const Message* m,
                                                        PickleIterator* iter,
                                                        param_type* p) {
  content::SyntheticGestureParams::GestureType gesture_type;
  if (!ReadParam(m, iter, &gesture_type))
    return false;
  scoped_ptr<content::SyntheticGestureParams> gesture_params;
  switch (gesture_type) {
    case content::SyntheticGestureParams::SMOOTH_SCROLL_GESTURE:
      gesture_params =
          ReadGestureParams<content::SyntheticSmoothScrollGestureParams>(m,
                                                                         iter);
      break;
    case content::SyntheticGestureParams::PINCH_GESTURE:
      gesture_params =
          ReadGestureParams<content::SyntheticPinchGestureParams>(m, iter);
      break;
    case content::SyntheticGestureParams::TAP_GESTURE:
      gesture_params =
          ReadGestureParams<content::SyntheticTapGestureParams>(m, iter);
      break;
    default:
      return false;
  }

  p->set_gesture_params(gesture_params.Pass());
  return p->gesture_params() != NULL;
}

void ParamTraits<content::SyntheticGesturePacket>::Log(const param_type& p,
                                                       std::string* l) {
  DCHECK(p.gesture_params());
  switch (p.gesture_params()->GetGestureType()) {
    case content::SyntheticGestureParams::SMOOTH_SCROLL_GESTURE:
      LogParam(
          *content::SyntheticSmoothScrollGestureParams::Cast(
              p.gesture_params()),
          l);
      break;
    case content::SyntheticGestureParams::PINCH_GESTURE:
      LogParam(
          *content::SyntheticPinchGestureParams::Cast(p.gesture_params()),
          l);
      break;
    case content::SyntheticGestureParams::TAP_GESTURE:
      LogParam(
          *content::SyntheticTapGestureParams::Cast(p.gesture_params()),
          l);
      break;
  }
}

}  // namespace IPC
