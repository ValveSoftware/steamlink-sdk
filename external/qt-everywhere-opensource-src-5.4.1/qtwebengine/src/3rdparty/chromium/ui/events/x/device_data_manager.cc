// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/x/device_data_manager.h"

#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>
#include <X11/Xlib.h>

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/sys_info.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_switches.h"
#include "ui/events/x/device_list_cache_x.h"
#include "ui/events/x/touch_factory_x11.h"
#include "ui/gfx/display.h"
#include "ui/gfx/point3_f.h"
#include "ui/gfx/x/x11_types.h"

// XIScrollClass was introduced in XI 2.1 so we need to define it here
// for backward-compatibility with older versions of XInput.
#if !defined(XIScrollClass)
#define XIScrollClass 3
#endif

// Multi-touch support was introduced in XI 2.2. Add XI event types here
// for backward-compatibility with older versions of XInput.
#if !defined(XI_TouchBegin)
#define XI_TouchBegin  18
#define XI_TouchUpdate 19
#define XI_TouchEnd    20
#endif

// Copied from xserver-properties.h
#define AXIS_LABEL_PROP_REL_HWHEEL "Rel Horiz Wheel"
#define AXIS_LABEL_PROP_REL_WHEEL "Rel Vert Wheel"

// CMT specific timings
#define AXIS_LABEL_PROP_ABS_DBL_START_TIME "Abs Dbl Start Timestamp"
#define AXIS_LABEL_PROP_ABS_DBL_END_TIME   "Abs Dbl End Timestamp"

// Ordinal values
#define AXIS_LABEL_PROP_ABS_DBL_ORDINAL_X   "Abs Dbl Ordinal X"
#define AXIS_LABEL_PROP_ABS_DBL_ORDINAL_Y   "Abs Dbl Ordinal Y"

// Fling properties
#define AXIS_LABEL_PROP_ABS_DBL_FLING_VX   "Abs Dbl Fling X Velocity"
#define AXIS_LABEL_PROP_ABS_DBL_FLING_VY   "Abs Dbl Fling Y Velocity"
#define AXIS_LABEL_PROP_ABS_FLING_STATE   "Abs Fling State"

#define AXIS_LABEL_PROP_ABS_FINGER_COUNT   "Abs Finger Count"

// Cros metrics gesture from touchpad
#define AXIS_LABEL_PROP_ABS_METRICS_TYPE      "Abs Metrics Type"
#define AXIS_LABEL_PROP_ABS_DBL_METRICS_DATA1 "Abs Dbl Metrics Data 1"
#define AXIS_LABEL_PROP_ABS_DBL_METRICS_DATA2 "Abs Dbl Metrics Data 2"

// Touchscreen multi-touch
#define AXIS_LABEL_ABS_MT_TOUCH_MAJOR "Abs MT Touch Major"
#define AXIS_LABEL_ABS_MT_TOUCH_MINOR "Abs MT Touch Minor"
#define AXIS_LABEL_ABS_MT_ORIENTATION "Abs MT Orientation"
#define AXIS_LABEL_ABS_MT_PRESSURE    "Abs MT Pressure"
#define AXIS_LABEL_ABS_MT_TRACKING_ID "Abs MT Tracking ID"
#define AXIS_LABEL_TOUCH_TIMESTAMP    "Touch Timestamp"

// When you add new data types, please make sure the order here is aligned
// with the order in the DataType enum in the header file because we assume
// they are in sync when updating the device list (see UpdateDeviceList).
const char* kCachedAtoms[] = {
  AXIS_LABEL_PROP_REL_HWHEEL,
  AXIS_LABEL_PROP_REL_WHEEL,
  AXIS_LABEL_PROP_ABS_DBL_ORDINAL_X,
  AXIS_LABEL_PROP_ABS_DBL_ORDINAL_Y,
  AXIS_LABEL_PROP_ABS_DBL_START_TIME,
  AXIS_LABEL_PROP_ABS_DBL_END_TIME,
  AXIS_LABEL_PROP_ABS_DBL_FLING_VX,
  AXIS_LABEL_PROP_ABS_DBL_FLING_VY,
  AXIS_LABEL_PROP_ABS_FLING_STATE,
  AXIS_LABEL_PROP_ABS_METRICS_TYPE,
  AXIS_LABEL_PROP_ABS_DBL_METRICS_DATA1,
  AXIS_LABEL_PROP_ABS_DBL_METRICS_DATA2,
  AXIS_LABEL_PROP_ABS_FINGER_COUNT,
  AXIS_LABEL_ABS_MT_TOUCH_MAJOR,
  AXIS_LABEL_ABS_MT_TOUCH_MINOR,
  AXIS_LABEL_ABS_MT_ORIENTATION,
  AXIS_LABEL_ABS_MT_PRESSURE,
  AXIS_LABEL_ABS_MT_TRACKING_ID,
  AXIS_LABEL_TOUCH_TIMESTAMP,

  NULL
};

// Constants for checking if a data type lies in the range of CMT/Touch data
// types.
const int kCMTDataTypeStart = ui::DeviceDataManager::DT_CMT_SCROLL_X;
const int kCMTDataTypeEnd = ui::DeviceDataManager::DT_CMT_FINGER_COUNT;
const int kTouchDataTypeStart = ui::DeviceDataManager::DT_TOUCH_MAJOR;
const int kTouchDataTypeEnd = ui::DeviceDataManager::DT_TOUCH_RAW_TIMESTAMP;

namespace ui {

bool DeviceDataManager::IsCMTDataType(const int type) {
  return (type >= kCMTDataTypeStart) && (type <= kCMTDataTypeEnd);
}

bool DeviceDataManager::IsTouchDataType(const int type) {
  return (type >= kTouchDataTypeStart) && (type <= kTouchDataTypeEnd);
}

DeviceDataManager* DeviceDataManager::GetInstance() {
  return Singleton<DeviceDataManager>::get();
}

DeviceDataManager::DeviceDataManager()
    : xi_opcode_(-1),
      atom_cache_(gfx::GetXDisplay(), kCachedAtoms),
      button_map_count_(0) {
  CHECK(gfx::GetXDisplay());
  InitializeXInputInternal();

  // Make sure the sizes of enum and kCachedAtoms are aligned.
  CHECK(arraysize(kCachedAtoms) == static_cast<size_t>(DT_LAST_ENTRY) + 1);
  UpdateDeviceList(gfx::GetXDisplay());
  UpdateButtonMap();
  for (int i = 0; i < kMaxDeviceNum; i++)
    touch_device_to_display_map_[i] = gfx::Display::kInvalidDisplayID;
}

DeviceDataManager::~DeviceDataManager() {
}

bool DeviceDataManager::InitializeXInputInternal() {
  // Check if XInput is available on the system.
  xi_opcode_ = -1;
  int opcode, event, error;
  if (!XQueryExtension(
      gfx::GetXDisplay(), "XInputExtension", &opcode, &event, &error)) {
    VLOG(1) << "X Input extension not available: error=" << error;
    return false;
  }

  // Check the XInput version.
#if defined(USE_XI2_MT)
  int major = 2, minor = USE_XI2_MT;
#else
  int major = 2, minor = 0;
#endif
  if (XIQueryVersion(gfx::GetXDisplay(), &major, &minor) == BadRequest) {
    VLOG(1) << "XInput2 not supported in the server.";
    return false;
  }
#if defined(USE_XI2_MT)
  if (major < 2 || (major == 2 && minor < USE_XI2_MT)) {
    DVLOG(1) << "XI version on server is " << major << "." << minor << ". "
            << "But 2." << USE_XI2_MT << " is required.";
    return false;
  }
#endif

  xi_opcode_ = opcode;
  CHECK_NE(-1, xi_opcode_);

  // Possible XI event types for XIDeviceEvent. See the XI2 protocol
  // specification.
  xi_device_event_types_[XI_KeyPress] = true;
  xi_device_event_types_[XI_KeyRelease] = true;
  xi_device_event_types_[XI_ButtonPress] = true;
  xi_device_event_types_[XI_ButtonRelease] = true;
  xi_device_event_types_[XI_Motion] = true;
  // Multi-touch support was introduced in XI 2.2.
  if (minor >= 2) {
    xi_device_event_types_[XI_TouchBegin] = true;
    xi_device_event_types_[XI_TouchUpdate] = true;
    xi_device_event_types_[XI_TouchEnd] = true;
  }
  return true;
}

bool DeviceDataManager::IsXInput2Available() const {
  return xi_opcode_ != -1;
}

void DeviceDataManager::UpdateDeviceList(Display* display) {
  cmt_devices_.reset();
  touchpads_.reset();
  for (int i = 0; i < kMaxDeviceNum; ++i) {
    valuator_count_[i] = 0;
    valuator_lookup_[i].clear();
    data_type_lookup_[i].clear();
    valuator_min_[i].clear();
    valuator_max_[i].clear();
    for (int j = 0; j < kMaxSlotNum; j++)
      last_seen_valuator_[i][j].clear();
  }

  // Find all the touchpad devices.
  XDeviceList dev_list =
      ui::DeviceListCacheX::GetInstance()->GetXDeviceList(display);
  Atom xi_touchpad = XInternAtom(display, XI_TOUCHPAD, false);
  for (int i = 0; i < dev_list.count; ++i)
    if (dev_list[i].type == xi_touchpad)
      touchpads_[dev_list[i].id] = true;

  if (!IsXInput2Available())
    return;

  // Update the structs with new valuator information
  XIDeviceList info_list =
      ui::DeviceListCacheX::GetInstance()->GetXI2DeviceList(display);
  Atom atoms[DT_LAST_ENTRY];
  for (int data_type = 0; data_type < DT_LAST_ENTRY; ++data_type)
    atoms[data_type] = atom_cache_.GetAtom(kCachedAtoms[data_type]);

  for (int i = 0; i < info_list.count; ++i) {
    XIDeviceInfo* info = info_list.devices + i;

    // We currently handle only slave, non-keyboard devices
    if (info->use != XISlavePointer && info->use != XIFloatingSlave)
      continue;

    bool possible_cmt = false;
    bool not_cmt = false;
    const int deviceid = info->deviceid;

    for (int j = 0; j < info->num_classes; ++j) {
      if (info->classes[j]->type == XIValuatorClass)
        ++valuator_count_[deviceid];
      else if (info->classes[j]->type == XIScrollClass)
        not_cmt = true;
    }

    // Skip devices that don't use any valuator
    if (!valuator_count_[deviceid])
      continue;

    valuator_lookup_[deviceid].resize(DT_LAST_ENTRY, -1);
    data_type_lookup_[deviceid].resize(
        valuator_count_[deviceid], DT_LAST_ENTRY);
    valuator_min_[deviceid].resize(DT_LAST_ENTRY, 0);
    valuator_max_[deviceid].resize(DT_LAST_ENTRY, 0);
    for (int j = 0; j < kMaxSlotNum; j++)
      last_seen_valuator_[deviceid][j].resize(DT_LAST_ENTRY, 0);
    for (int j = 0; j < info->num_classes; ++j) {
      if (info->classes[j]->type != XIValuatorClass)
        continue;

      XIValuatorClassInfo* v =
          reinterpret_cast<XIValuatorClassInfo*>(info->classes[j]);
      for (int data_type = 0; data_type < DT_LAST_ENTRY; ++data_type) {
        if (v->label == atoms[data_type]) {
          valuator_lookup_[deviceid][data_type] = v->number;
          data_type_lookup_[deviceid][v->number] = data_type;
          valuator_min_[deviceid][data_type] = v->min;
          valuator_max_[deviceid][data_type] = v->max;
          if (IsCMTDataType(data_type))
            possible_cmt = true;
          break;
        }
      }
    }

    if (possible_cmt && !not_cmt)
      cmt_devices_[deviceid] = true;
  }
}

bool DeviceDataManager::GetSlotNumber(const XIDeviceEvent* xiev, int* slot) {
#if defined(USE_XI2_MT)
  ui::TouchFactory* factory = ui::TouchFactory::GetInstance();
  if (!factory->IsMultiTouchDevice(xiev->sourceid)) {
    *slot = 0;
    return true;
  }
  return factory->QuerySlotForTrackingID(xiev->detail, slot);
#else
  *slot = 0;
  return true;
#endif
}

void DeviceDataManager::GetEventRawData(const XEvent& xev, EventData* data) {
  if (xev.type != GenericEvent)
    return;

  XIDeviceEvent* xiev = static_cast<XIDeviceEvent*>(xev.xcookie.data);
  if (xiev->sourceid >= kMaxDeviceNum || xiev->deviceid >= kMaxDeviceNum)
    return;
  data->clear();
  const int sourceid = xiev->sourceid;
  double* valuators = xiev->valuators.values;
  for (int i = 0; i <= valuator_count_[sourceid]; ++i) {
    if (XIMaskIsSet(xiev->valuators.mask, i)) {
      int type = data_type_lookup_[sourceid][i];
      if (type != DT_LAST_ENTRY) {
        (*data)[type] = *valuators;
        if (IsTouchDataType(type)) {
          int slot = -1;
          if (GetSlotNumber(xiev, &slot) && slot >= 0 && slot < kMaxSlotNum)
            last_seen_valuator_[sourceid][slot][type] = *valuators;
        }
      }
      valuators++;
    }
  }
}

bool DeviceDataManager::GetEventData(const XEvent& xev,
    const DataType type, double* value) {
  if (xev.type != GenericEvent)
    return false;

  XIDeviceEvent* xiev = static_cast<XIDeviceEvent*>(xev.xcookie.data);
  if (xiev->sourceid >= kMaxDeviceNum || xiev->deviceid >= kMaxDeviceNum)
    return false;
  const int sourceid = xiev->sourceid;
  if (valuator_lookup_[sourceid].empty())
    return false;

  if (type == DT_TOUCH_TRACKING_ID) {
    // With XInput2 MT, Tracking ID is provided in the detail field for touch
    // events.
    if (xiev->evtype == XI_TouchBegin ||
        xiev->evtype == XI_TouchEnd ||
        xiev->evtype == XI_TouchUpdate) {
      *value = xiev->detail;
    } else {
      *value = 0;
    }
    return true;
  }

  int val_index = valuator_lookup_[sourceid][type];
  int slot = 0;
  if (val_index >= 0) {
    if (XIMaskIsSet(xiev->valuators.mask, val_index)) {
      double* valuators = xiev->valuators.values;
      while (val_index--) {
        if (XIMaskIsSet(xiev->valuators.mask, val_index))
          ++valuators;
      }
      *value = *valuators;
      if (IsTouchDataType(type)) {
        if (GetSlotNumber(xiev, &slot) && slot >= 0 && slot < kMaxSlotNum)
          last_seen_valuator_[sourceid][slot][type] = *value;
      }
      return true;
    } else if (IsTouchDataType(type)) {
      if (GetSlotNumber(xiev, &slot) && slot >= 0 && slot < kMaxSlotNum)
        *value = last_seen_valuator_[sourceid][slot][type];
    }
  }

  return false;
}

bool DeviceDataManager::IsXIDeviceEvent(
    const base::NativeEvent& native_event) const {
  if (native_event->type != GenericEvent ||
      native_event->xcookie.extension != xi_opcode_)
    return false;
  return xi_device_event_types_[native_event->xcookie.evtype];
}

bool DeviceDataManager::IsTouchpadXInputEvent(
    const base::NativeEvent& native_event) const {
  if (native_event->type != GenericEvent)
    return false;

  XIDeviceEvent* xievent =
      static_cast<XIDeviceEvent*>(native_event->xcookie.data);
  if (xievent->sourceid >= kMaxDeviceNum)
    return false;
  return touchpads_[xievent->sourceid];
}

bool DeviceDataManager::IsCMTDeviceEvent(
    const base::NativeEvent& native_event) const {
  if (native_event->type != GenericEvent)
    return false;

  XIDeviceEvent* xievent =
      static_cast<XIDeviceEvent*>(native_event->xcookie.data);
  if (xievent->sourceid >= kMaxDeviceNum)
    return false;
  return cmt_devices_[xievent->sourceid];
}

bool DeviceDataManager::IsCMTGestureEvent(
    const base::NativeEvent& native_event) const {
  return (IsScrollEvent(native_event) ||
          IsFlingEvent(native_event) ||
          IsCMTMetricsEvent(native_event));
}

bool DeviceDataManager::HasEventData(
    const XIDeviceEvent* xiev, const DataType type) const {
  const int idx = valuator_lookup_[xiev->sourceid][type];
  return (idx >= 0) && XIMaskIsSet(xiev->valuators.mask, idx);
}

bool DeviceDataManager::IsScrollEvent(
    const base::NativeEvent& native_event) const {
  if (!IsCMTDeviceEvent(native_event))
    return false;

  XIDeviceEvent* xiev =
      static_cast<XIDeviceEvent*>(native_event->xcookie.data);
  return (HasEventData(xiev, DT_CMT_SCROLL_X) ||
          HasEventData(xiev, DT_CMT_SCROLL_Y));
}

bool DeviceDataManager::IsFlingEvent(
    const base::NativeEvent& native_event) const {
  if (!IsCMTDeviceEvent(native_event))
    return false;

  XIDeviceEvent* xiev =
      static_cast<XIDeviceEvent*>(native_event->xcookie.data);
  return (HasEventData(xiev, DT_CMT_FLING_X) &&
          HasEventData(xiev, DT_CMT_FLING_Y) &&
          HasEventData(xiev, DT_CMT_FLING_STATE));
}

bool DeviceDataManager::IsCMTMetricsEvent(
    const base::NativeEvent& native_event) const {
  if (!IsCMTDeviceEvent(native_event))
    return false;

  XIDeviceEvent* xiev =
      static_cast<XIDeviceEvent*>(native_event->xcookie.data);
  return (HasEventData(xiev, DT_CMT_METRICS_TYPE) &&
          HasEventData(xiev, DT_CMT_METRICS_DATA1) &&
          HasEventData(xiev, DT_CMT_METRICS_DATA2));
}

bool DeviceDataManager::HasGestureTimes(
    const base::NativeEvent& native_event) const {
  if (!IsCMTDeviceEvent(native_event))
    return false;

  XIDeviceEvent* xiev =
      static_cast<XIDeviceEvent*>(native_event->xcookie.data);
  return (HasEventData(xiev, DT_CMT_START_TIME) &&
          HasEventData(xiev, DT_CMT_END_TIME));
}

void DeviceDataManager::GetScrollOffsets(const base::NativeEvent& native_event,
                                         float* x_offset, float* y_offset,
                                         float* x_offset_ordinal,
                                         float* y_offset_ordinal,
                                         int* finger_count) {
  *x_offset = 0;
  *y_offset = 0;
  *x_offset_ordinal = 0;
  *y_offset_ordinal = 0;
  *finger_count = 2;

  EventData data;
  GetEventRawData(*native_event, &data);

  if (data.find(DT_CMT_SCROLL_X) != data.end())
    *x_offset = data[DT_CMT_SCROLL_X];
  if (data.find(DT_CMT_SCROLL_Y) != data.end())
    *y_offset = data[DT_CMT_SCROLL_Y];
  if (data.find(DT_CMT_ORDINAL_X) != data.end())
    *x_offset_ordinal = data[DT_CMT_ORDINAL_X];
  if (data.find(DT_CMT_ORDINAL_Y) != data.end())
    *y_offset_ordinal = data[DT_CMT_ORDINAL_Y];
  if (data.find(DT_CMT_FINGER_COUNT) != data.end())
    *finger_count = static_cast<int>(data[DT_CMT_FINGER_COUNT]);
}

void DeviceDataManager::GetFlingData(const base::NativeEvent& native_event,
                                     float* vx, float* vy,
                                     float* vx_ordinal, float* vy_ordinal,
                                     bool* is_cancel) {
  *vx = 0;
  *vy = 0;
  *vx_ordinal = 0;
  *vy_ordinal = 0;
  *is_cancel = false;

  EventData data;
  GetEventRawData(*native_event, &data);

  if (data.find(DT_CMT_FLING_X) != data.end())
    *vx = data[DT_CMT_FLING_X];
  if (data.find(DT_CMT_FLING_Y) != data.end())
    *vy = data[DT_CMT_FLING_Y];
  if (data.find(DT_CMT_FLING_STATE) != data.end())
    *is_cancel = !!static_cast<unsigned int>(data[DT_CMT_FLING_STATE]);
  if (data.find(DT_CMT_ORDINAL_X) != data.end())
    *vx_ordinal = data[DT_CMT_ORDINAL_X];
  if (data.find(DT_CMT_ORDINAL_Y) != data.end())
    *vy_ordinal = data[DT_CMT_ORDINAL_Y];
}

void DeviceDataManager::GetMetricsData(const base::NativeEvent& native_event,
                                       GestureMetricsType* type,
                                       float* data1, float* data2) {
  *type = kGestureMetricsTypeUnknown;
  *data1 = 0;
  *data2 = 0;

  EventData data;
  GetEventRawData(*native_event, &data);

  if (data.find(DT_CMT_METRICS_TYPE) != data.end()) {
    int val = static_cast<int>(data[DT_CMT_METRICS_TYPE]);
    if (val == 0)
      *type = kGestureMetricsTypeNoisyGround;
    else
      *type = kGestureMetricsTypeUnknown;
  }
  if (data.find(DT_CMT_METRICS_DATA1) != data.end())
    *data1 = data[DT_CMT_METRICS_DATA1];
  if (data.find(DT_CMT_METRICS_DATA2) != data.end())
    *data2 = data[DT_CMT_METRICS_DATA2];
}

int DeviceDataManager::GetMappedButton(int button) {
  return button > 0 && button <= button_map_count_ ? button_map_[button - 1] :
                                                     button;
}

void DeviceDataManager::UpdateButtonMap() {
  button_map_count_ = XGetPointerMapping(gfx::GetXDisplay(),
                                         button_map_,
                                         arraysize(button_map_));
}

void DeviceDataManager::GetGestureTimes(const base::NativeEvent& native_event,
                                        double* start_time,
                                        double* end_time) {
  *start_time = 0;
  *end_time = 0;

  EventData data;
  GetEventRawData(*native_event, &data);

  if (data.find(DT_CMT_START_TIME) != data.end())
    *start_time = data[DT_CMT_START_TIME];
  if (data.find(DT_CMT_END_TIME) != data.end())
    *end_time = data[DT_CMT_END_TIME];
}

bool DeviceDataManager::NormalizeData(unsigned int deviceid,
                                      const DataType type,
                                      double* value) {
  double max_value;
  double min_value;
  if (GetDataRange(deviceid, type, &min_value, &max_value)) {
    *value = (*value - min_value) / (max_value - min_value);
    DCHECK(*value >= 0.0 && *value <= 1.0);
    return true;
  }
  return false;
}

bool DeviceDataManager::GetDataRange(unsigned int deviceid,
                                     const DataType type,
                                     double* min, double* max) {
  if (deviceid >= static_cast<unsigned int>(kMaxDeviceNum))
    return false;
  if (valuator_lookup_[deviceid][type] >= 0) {
    *min = valuator_min_[deviceid][type];
    *max = valuator_max_[deviceid][type];
    return true;
  }
  return false;
}

void DeviceDataManager::SetDeviceListForTest(
    const std::vector<unsigned int>& touchscreen,
    const std::vector<unsigned int>& cmt_devices) {
  for (int i = 0; i < kMaxDeviceNum; ++i) {
    valuator_count_[i] = 0;
    valuator_lookup_[i].clear();
    data_type_lookup_[i].clear();
    valuator_min_[i].clear();
    valuator_max_[i].clear();
    for (int j = 0; j < kMaxSlotNum; j++)
      last_seen_valuator_[i][j].clear();
  }

  for (size_t i = 0; i < touchscreen.size(); i++) {
    unsigned int deviceid = touchscreen[i];
    InitializeValuatorsForTest(deviceid, kTouchDataTypeStart, kTouchDataTypeEnd,
                               0, 1000);
  }

  cmt_devices_.reset();
  for (size_t i = 0; i < cmt_devices.size(); ++i) {
    unsigned int deviceid = cmt_devices[i];
    cmt_devices_[deviceid] = true;
    touchpads_[deviceid] = true;
    InitializeValuatorsForTest(deviceid, kCMTDataTypeStart, kCMTDataTypeEnd,
                               -1000, 1000);
  }
}

void DeviceDataManager::SetValuatorDataForTest(XIDeviceEvent* xievent,
                                               DataType type,
                                               double value) {
  int index = valuator_lookup_[xievent->deviceid][type];
  CHECK(!XIMaskIsSet(xievent->valuators.mask, index));
  CHECK(index >= 0 && index < valuator_count_[xievent->deviceid]);
  XISetMask(xievent->valuators.mask, index);

  double* valuators = xievent->valuators.values;
  for (int i = 0; i < index; ++i) {
    if (XIMaskIsSet(xievent->valuators.mask, i))
      valuators++;
  }
  for (int i = DT_LAST_ENTRY - 1; i > valuators - xievent->valuators.values;
       --i)
    xievent->valuators.values[i] = xievent->valuators.values[i - 1];
  *valuators = value;
}

void DeviceDataManager::InitializeValuatorsForTest(int deviceid,
                                                   int start_valuator,
                                                   int end_valuator,
                                                   double min_value,
                                                   double max_value) {
  valuator_lookup_[deviceid].resize(DT_LAST_ENTRY, -1);
  data_type_lookup_[deviceid].resize(DT_LAST_ENTRY, DT_LAST_ENTRY);
  valuator_min_[deviceid].resize(DT_LAST_ENTRY, 0);
  valuator_max_[deviceid].resize(DT_LAST_ENTRY, 0);
  for (int j = 0; j < kMaxSlotNum; j++)
    last_seen_valuator_[deviceid][j].resize(DT_LAST_ENTRY, 0);
  for (int j = start_valuator; j <= end_valuator; ++j) {
    valuator_lookup_[deviceid][j] = valuator_count_[deviceid];
    data_type_lookup_[deviceid][valuator_count_[deviceid]] = j;
    valuator_min_[deviceid][j] = min_value;
    valuator_max_[deviceid][j] = max_value;
    valuator_count_[deviceid]++;
  }
}

bool DeviceDataManager::TouchEventNeedsCalibrate(int touch_device_id) const {
#if defined(OS_CHROMEOS) && defined(USE_XI2_MT)
  int64 touch_display_id = GetDisplayForTouchDevice(touch_device_id);
  if (base::SysInfo::IsRunningOnChromeOS() &&
      touch_display_id == gfx::Display::InternalDisplayId()) {
    return true;
  }
#endif  // defined(OS_CHROMEOS) && defined(USE_XI2_MT)
  return false;
}

void DeviceDataManager::ClearTouchTransformerRecord() {
  for (int i = 0; i < kMaxDeviceNum; i++) {
    touch_device_transformer_map_[i] = gfx::Transform();
    touch_device_to_display_map_[i] = gfx::Display::kInvalidDisplayID;
  }
}

bool DeviceDataManager::IsTouchDeviceIdValid(int touch_device_id) const {
  return (touch_device_id > 0 && touch_device_id < kMaxDeviceNum);
}

void DeviceDataManager::UpdateTouchInfoForDisplay(
    int64 display_id,
    int touch_device_id,
    const gfx::Transform& touch_transformer) {
  if (IsTouchDeviceIdValid(touch_device_id)) {
    touch_device_to_display_map_[touch_device_id] = display_id;
    touch_device_transformer_map_[touch_device_id] = touch_transformer;
  }
}

void DeviceDataManager::ApplyTouchTransformer(int touch_device_id,
                                              float* x, float* y) {
  if (IsTouchDeviceIdValid(touch_device_id)) {
    gfx::Point3F point(*x, *y, 0.0);
    const gfx::Transform& trans =
        touch_device_transformer_map_[touch_device_id];
    trans.TransformPoint(&point);
    *x = point.x();
    *y = point.y();
  }
}

int64 DeviceDataManager::GetDisplayForTouchDevice(int touch_device_id) const {
  if (IsTouchDeviceIdValid(touch_device_id))
    return touch_device_to_display_map_[touch_device_id];
  return gfx::Display::kInvalidDisplayID;
}

}  // namespace ui
