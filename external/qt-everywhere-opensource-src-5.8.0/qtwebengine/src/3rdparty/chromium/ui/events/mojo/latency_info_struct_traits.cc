// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/mojo/latency_info_struct_traits.h"

#include "ipc/ipc_message_utils.h"

namespace mojo {

namespace {

ui::mojom::LatencyComponentType UILatencyComponentTypeToMojo(
    ui::LatencyComponentType type) {
  switch (type) {
    case ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT;
    case ui::LATENCY_BEGIN_SCROLL_LISTENER_UPDATE_MAIN_COMPONENT:
      return ui::mojom::LatencyComponentType::
          LATENCY_BEGIN_SCROLL_LISTENER_UPDATE_MAIN_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_UI_COMPONENT:
      return ui::mojom::LatencyComponentType::INPUT_EVENT_LATENCY_UI_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_RENDERER_MAIN_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_RENDERER_MAIN_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_MAIN_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_MAIN_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_IMPL_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_IMPL_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_FORWARD_SCROLL_UPDATE_TO_MAIN_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_FORWARD_SCROLL_UPDATE_TO_MAIN_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_ACK_RWH_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_ACK_RWH_COMPONENT;
    case ui::WINDOW_SNAPSHOT_FRAME_NUMBER_COMPONENT:
      return ui::mojom::LatencyComponentType::
          WINDOW_SNAPSHOT_FRAME_NUMBER_COMPONENT;
    case ui::TAB_SHOW_COMPONENT:
      return ui::mojom::LatencyComponentType::TAB_SHOW_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT;
    case ui::INPUT_EVENT_BROWSER_RECEIVED_RENDERER_SWAP_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_BROWSER_RECEIVED_RENDERER_SWAP_COMPONENT;
    case ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_GENERATE_SCROLL_UPDATE_FROM_MOUSE_WHEEL:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_GENERATE_SCROLL_UPDATE_FROM_MOUSE_WHEEL;
    case ui::INPUT_EVENT_LATENCY_TERMINATED_MOUSE_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_TERMINATED_MOUSE_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_TERMINATED_MOUSE_WHEEL_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_TERMINATED_MOUSE_WHEEL_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_TERMINATED_KEYBOARD_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_TERMINATED_KEYBOARD_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_TERMINATED_TOUCH_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_TERMINATED_TOUCH_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_TERMINATED_GESTURE_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_TERMINATED_GESTURE_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_TERMINATED_FRAME_SWAP_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_TERMINATED_FRAME_SWAP_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_TERMINATED_COMMIT_FAILED_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_TERMINATED_COMMIT_FAILED_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_TERMINATED_COMMIT_NO_UPDATE_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_TERMINATED_COMMIT_NO_UPDATE_COMPONENT;
    case ui::INPUT_EVENT_LATENCY_TERMINATED_SWAP_FAILED_COMPONENT:
      return ui::mojom::LatencyComponentType::
          INPUT_EVENT_LATENCY_TERMINATED_SWAP_FAILED_COMPONENT;
  }
  NOTREACHED();
  return ui::mojom::LatencyComponentType::LATENCY_COMPONENT_TYPE_LAST;
}

ui::LatencyComponentType MojoLatencyComponentTypeToUI(
    ui::mojom::LatencyComponentType type) {
  switch (type) {
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT:
      return ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT;
    case ui::mojom::LatencyComponentType::
        LATENCY_BEGIN_SCROLL_LISTENER_UPDATE_MAIN_COMPONENT:
      return ui::LATENCY_BEGIN_SCROLL_LISTENER_UPDATE_MAIN_COMPONENT;
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT:
      return ui::INPUT_EVENT_LATENCY_SCROLL_UPDATE_ORIGINAL_COMPONENT;
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT:
      return ui::INPUT_EVENT_LATENCY_FIRST_SCROLL_UPDATE_ORIGINAL_COMPONENT;
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT:
      return ui::INPUT_EVENT_LATENCY_ORIGINAL_COMPONENT;
    case ui::mojom::LatencyComponentType::INPUT_EVENT_LATENCY_UI_COMPONENT:
      return ui::INPUT_EVENT_LATENCY_UI_COMPONENT;
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_RENDERER_MAIN_COMPONENT:
      return ui::INPUT_EVENT_LATENCY_RENDERER_MAIN_COMPONENT;
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_MAIN_COMPONENT:
      return ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_MAIN_COMPONENT;
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_IMPL_COMPONENT:
      return ui::INPUT_EVENT_LATENCY_RENDERING_SCHEDULED_IMPL_COMPONENT;
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_FORWARD_SCROLL_UPDATE_TO_MAIN_COMPONENT:
      return ui::INPUT_EVENT_LATENCY_FORWARD_SCROLL_UPDATE_TO_MAIN_COMPONENT;
    case ui::mojom::LatencyComponentType::INPUT_EVENT_LATENCY_ACK_RWH_COMPONENT:
      return ui::INPUT_EVENT_LATENCY_ACK_RWH_COMPONENT;
    case ui::mojom::LatencyComponentType::
        WINDOW_SNAPSHOT_FRAME_NUMBER_COMPONENT:
      return ui::WINDOW_SNAPSHOT_FRAME_NUMBER_COMPONENT;
    case ui::mojom::LatencyComponentType::TAB_SHOW_COMPONENT:
      return ui::TAB_SHOW_COMPONENT;
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT:
      return ui::INPUT_EVENT_LATENCY_RENDERER_SWAP_COMPONENT;
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_BROWSER_RECEIVED_RENDERER_SWAP_COMPONENT:
      return ui::INPUT_EVENT_BROWSER_RECEIVED_RENDERER_SWAP_COMPONENT;
    case ui::mojom::LatencyComponentType::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT:
      return ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT;
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_GENERATE_SCROLL_UPDATE_FROM_MOUSE_WHEEL:
      return ui::INPUT_EVENT_LATENCY_GENERATE_SCROLL_UPDATE_FROM_MOUSE_WHEEL;
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_TERMINATED_MOUSE_COMPONENT:
      return ui::INPUT_EVENT_LATENCY_TERMINATED_MOUSE_COMPONENT;
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_TERMINATED_MOUSE_WHEEL_COMPONENT:
      return ui::INPUT_EVENT_LATENCY_TERMINATED_MOUSE_WHEEL_COMPONENT;
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_TERMINATED_KEYBOARD_COMPONENT:
      return ui::INPUT_EVENT_LATENCY_TERMINATED_KEYBOARD_COMPONENT;
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_TERMINATED_TOUCH_COMPONENT:
      return ui::INPUT_EVENT_LATENCY_TERMINATED_TOUCH_COMPONENT;
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_TERMINATED_GESTURE_COMPONENT:
      return ui::INPUT_EVENT_LATENCY_TERMINATED_GESTURE_COMPONENT;
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_TERMINATED_FRAME_SWAP_COMPONENT:
      return ui::INPUT_EVENT_LATENCY_TERMINATED_FRAME_SWAP_COMPONENT;
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_TERMINATED_COMMIT_FAILED_COMPONENT:
      return ui::INPUT_EVENT_LATENCY_TERMINATED_COMMIT_FAILED_COMPONENT;
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_TERMINATED_COMMIT_NO_UPDATE_COMPONENT:
      return ui::INPUT_EVENT_LATENCY_TERMINATED_COMMIT_NO_UPDATE_COMPONENT;
    case ui::mojom::LatencyComponentType::
        INPUT_EVENT_LATENCY_TERMINATED_SWAP_FAILED_COMPONENT:
      return ui::INPUT_EVENT_LATENCY_TERMINATED_SWAP_FAILED_COMPONENT;
  }
  NOTREACHED();
  return ui::LATENCY_COMPONENT_TYPE_LAST;
}

}  // namespace

// static
int64_t
StructTraits<ui::mojom::LatencyComponent, ui::LatencyInfo::LatencyComponent>::
    sequence_number(const ui::LatencyInfo::LatencyComponent& component) {
  return component.sequence_number;
}

// static
base::TimeTicks
StructTraits<ui::mojom::LatencyComponent, ui::LatencyInfo::LatencyComponent>::
    event_time(const ui::LatencyInfo::LatencyComponent& component) {
  return component.event_time;
}

// static
uint32_t
StructTraits<ui::mojom::LatencyComponent, ui::LatencyInfo::LatencyComponent>::
    event_count(const ui::LatencyInfo::LatencyComponent& component) {
  return component.event_count;
}

// static
bool StructTraits<ui::mojom::LatencyComponent,
                  ui::LatencyInfo::LatencyComponent>::
    Read(ui::mojom::LatencyComponentDataView data,
         ui::LatencyInfo::LatencyComponent* out) {
  if (!data.ReadEventTime(&out->event_time))
    return false;
  out->sequence_number = data.sequence_number();
  out->event_count = data.event_count();
  return true;
}

// static
ui::mojom::LatencyComponentType
StructTraits<ui::mojom::LatencyComponentId,
             std::pair<ui::LatencyComponentType, int64_t>>::
    type(const std::pair<ui::LatencyComponentType, int64_t>& id) {
  return UILatencyComponentTypeToMojo(id.first);
}

// static
int64_t StructTraits<ui::mojom::LatencyComponentId,
                     std::pair<ui::LatencyComponentType, int64_t>>::
    id(const std::pair<ui::LatencyComponentType, int64_t>& id) {
  return id.second;
}

// static
bool StructTraits<ui::mojom::LatencyComponentId,
                  std::pair<ui::LatencyComponentType, int64_t>>::
    Read(ui::mojom::LatencyComponentIdDataView data,
         std::pair<ui::LatencyComponentType, int64_t>* out) {
  out->first = MojoLatencyComponentTypeToUI(data.type());
  out->second = data.id();
  return true;
}

// static
const std::string&
StructTraits<ui::mojom::LatencyInfo, ui::LatencyInfo>::trace_name(
    const ui::LatencyInfo& info) {
  return info.trace_name_;
}

const ui::LatencyInfo::LatencyMap&
StructTraits<ui::mojom::LatencyInfo, ui::LatencyInfo>::latency_components(
    const ui::LatencyInfo& info) {
  return info.latency_components();
}
InputCoordinateArray
StructTraits<ui::mojom::LatencyInfo, ui::LatencyInfo>::input_coordinates(
    const ui::LatencyInfo& info) {
  return {info.input_coordinates_size_, ui::LatencyInfo::kMaxInputCoordinates,
          const_cast<gfx::PointF*>(info.input_coordinates_)};
}

int64_t StructTraits<ui::mojom::LatencyInfo, ui::LatencyInfo>::trace_id(
    const ui::LatencyInfo& info) {
  return info.trace_id();
}

bool StructTraits<ui::mojom::LatencyInfo, ui::LatencyInfo>::coalesced(
    const ui::LatencyInfo& info) {
  return info.coalesced();
}

bool StructTraits<ui::mojom::LatencyInfo, ui::LatencyInfo>::terminated(
    const ui::LatencyInfo& info) {
  return info.terminated();
}

bool StructTraits<ui::mojom::LatencyInfo, ui::LatencyInfo>::Read(
    ui::mojom::LatencyInfoDataView data,
    ui::LatencyInfo* out) {
  if (!data.ReadTraceName(&out->trace_name_))
    return false;

  // TODO(fsamuel): Figure out how to optimize deserialization.
  mojo::Array<ui::mojom::LatencyComponentPairPtr> components;
  if (!data.ReadLatencyComponents(&components))
    return false;
  for (uint32_t i = 0; i < components.size(); ++i)
    out->latency_components_[components[i]->key] = components[i]->value;

  InputCoordinateArray input_coordinate_array = {
      0, ui::LatencyInfo::kMaxInputCoordinates, out->input_coordinates_};
  if (!data.ReadInputCoordinates(&input_coordinate_array))
    return false;
  // TODO(fsamuel): ui::LatencyInfo::input_coordinates_size_ should be a size_t.
  out->input_coordinates_size_ =
      static_cast<uint32_t>(input_coordinate_array.size);

  out->trace_id_ = data.trace_id();
  out->coalesced_ = data.coalesced();
  out->terminated_ = data.terminated();
  return true;
}

}  // namespace mojo
