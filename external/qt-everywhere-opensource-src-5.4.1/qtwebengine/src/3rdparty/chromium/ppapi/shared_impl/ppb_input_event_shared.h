// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPB_INPUT_EVENT_SHARED_H_
#define PPAPI_SHARED_IMPL_PPB_INPUT_EVENT_SHARED_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ppapi/c/ppb_input_event.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_input_event_api.h"
#include "ui/events/latency_info.h"

namespace ppapi {

// IF YOU ADD STUFF TO THIS CLASS
// ==============================
// Be sure to add it to the STRUCT_TRAITS at the top of ppapi_messages.h
struct PPAPI_SHARED_EXPORT InputEventData {
  InputEventData();
  ~InputEventData();

  // Internal-only value. Set to true when this input event is filtered, that
  // is, should be delivered synchronously. This is used by the proxy.
  bool is_filtered;

  PP_InputEvent_Type event_type;
  PP_TimeTicks event_time_stamp;
  uint32_t event_modifiers;

  PP_InputEvent_MouseButton mouse_button;
  PP_Point mouse_position;
  int32_t mouse_click_count;
  PP_Point mouse_movement;

  PP_FloatPoint wheel_delta;
  PP_FloatPoint wheel_ticks;
  bool wheel_scroll_by_page;

  uint32_t key_code;

  // The key event's |code| attribute as defined in:
  // http://www.w3.org/TR/uievents/
  std::string code;

  std::string character_text;

  std::vector<uint32_t> composition_segment_offsets;
  int32_t composition_target_segment;
  uint32_t composition_selection_start;
  uint32_t composition_selection_end;

  std::vector<PP_TouchPoint> touches;
  std::vector<PP_TouchPoint> changed_touches;
  std::vector<PP_TouchPoint> target_touches;

  ui::LatencyInfo latency_info;
};

// This simple class implements the PPB_InputEvent_API in terms of the
// shared InputEventData structure
class PPAPI_SHARED_EXPORT PPB_InputEvent_Shared
    : public Resource,
      public thunk::PPB_InputEvent_API {
 public:
  PPB_InputEvent_Shared(ResourceObjectType type,
                        PP_Instance instance,
                        const InputEventData& data);

  // Resource overrides.
  virtual PPB_InputEvent_API* AsPPB_InputEvent_API() OVERRIDE;

  // PPB_InputEvent_API implementation.
  virtual const InputEventData& GetInputEventData() const OVERRIDE;
  virtual PP_InputEvent_Type GetType() OVERRIDE;
  virtual PP_TimeTicks GetTimeStamp() OVERRIDE;
  virtual uint32_t GetModifiers() OVERRIDE;
  virtual PP_InputEvent_MouseButton GetMouseButton() OVERRIDE;
  virtual PP_Point GetMousePosition() OVERRIDE;
  virtual int32_t GetMouseClickCount() OVERRIDE;
  virtual PP_Point GetMouseMovement() OVERRIDE;
  virtual PP_FloatPoint GetWheelDelta() OVERRIDE;
  virtual PP_FloatPoint GetWheelTicks() OVERRIDE;
  virtual PP_Bool GetWheelScrollByPage() OVERRIDE;
  virtual uint32_t GetKeyCode() OVERRIDE;
  virtual PP_Var GetCharacterText() OVERRIDE;
  virtual PP_Var GetCode() OVERRIDE;
  virtual uint32_t GetIMESegmentNumber() OVERRIDE;
  virtual uint32_t GetIMESegmentOffset(uint32_t index) OVERRIDE;
  virtual int32_t GetIMETargetSegment() OVERRIDE;
  virtual void GetIMESelection(uint32_t* start, uint32_t* end) OVERRIDE;
  virtual void AddTouchPoint(PP_TouchListType list,
                             const PP_TouchPoint& point) OVERRIDE;
  virtual uint32_t GetTouchCount(PP_TouchListType list) OVERRIDE;
  virtual PP_TouchPoint GetTouchByIndex(PP_TouchListType list,
                                        uint32_t index) OVERRIDE;
  virtual PP_TouchPoint GetTouchById(PP_TouchListType list,
                                     uint32_t id) OVERRIDE;
  virtual PP_Bool TraceInputLatency(PP_Bool has_damage) OVERRIDE;

  // Implementations for event creation.
  static PP_Resource CreateIMEInputEvent(ResourceObjectType type,
                                         PP_Instance instance,
                                         PP_InputEvent_Type event_type,
                                         PP_TimeTicks time_stamp,
                                         struct PP_Var text,
                                         uint32_t segment_number,
                                         const uint32_t* segment_offsets,
                                         int32_t target_segment,
                                         uint32_t selection_start,
                                         uint32_t selection_end);
  static PP_Resource CreateKeyboardInputEvent(ResourceObjectType type,
                                              PP_Instance instance,
                                              PP_InputEvent_Type event_type,
                                              PP_TimeTicks time_stamp,
                                              uint32_t modifiers,
                                              uint32_t key_code,
                                              struct PP_Var character_text,
                                              struct PP_Var code);
  static PP_Resource CreateMouseInputEvent(
      ResourceObjectType type,
      PP_Instance instance,
      PP_InputEvent_Type event_type,
      PP_TimeTicks time_stamp,
      uint32_t modifiers,
      PP_InputEvent_MouseButton mouse_button,
      const PP_Point* mouse_position,
      int32_t click_count,
      const PP_Point* mouse_movement);
  static PP_Resource CreateWheelInputEvent(ResourceObjectType type,
                                           PP_Instance instance,
                                           PP_TimeTicks time_stamp,
                                           uint32_t modifiers,
                                           const PP_FloatPoint* wheel_delta,
                                           const PP_FloatPoint* wheel_ticks,
                                           PP_Bool scroll_by_page);
  static PP_Resource CreateTouchInputEvent(ResourceObjectType type,
                                           PP_Instance instance,
                                           PP_InputEvent_Type event_type,
                                           PP_TimeTicks time_stamp,
                                           uint32_t modifiers);

 private:
  InputEventData data_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(PPB_InputEvent_Shared);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPB_INPUT_EVENT_SHARED_H_
