// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_SYNTHETIC_POINTER_ACTION_PARAMS_H_
#define CONTENT_COMMON_INPUT_SYNTHETIC_POINTER_ACTION_PARAMS_H_

#include "base/logging.h"
#include "content/common/content_export.h"
#include "content/common/input/input_param_traits.h"
#include "content/common/input/synthetic_gesture_params.h"
#include "ui/gfx/geometry/point_f.h"

namespace ipc_fuzzer {
template <class T>
struct FuzzTraits;
}  // namespace ipc_fuzzer

namespace content {

struct CONTENT_EXPORT SyntheticPointerActionParams
    : public SyntheticGestureParams {
 public:
  // Actions are queued up until we receive a PROCESS action, at which point
  // we'll dispatch all queued events.
  enum class PointerActionType {
    NOT_INITIALIZED,
    PRESS,
    MOVE,
    RELEASE,
    PROCESS,
    POINTER_ACTION_TYPE_MAX = PROCESS
  };

  SyntheticPointerActionParams();
  explicit SyntheticPointerActionParams(PointerActionType type);
  SyntheticPointerActionParams(const SyntheticPointerActionParams& other);
  ~SyntheticPointerActionParams() override;

  GestureType GetGestureType() const override;

  static const SyntheticPointerActionParams* Cast(
      const SyntheticGestureParams* gesture_params);

  void set_pointer_action_type(PointerActionType pointer_action_type) {
    pointer_action_type_ = pointer_action_type;
  }

  void set_index(int index) {
    DCHECK(pointer_action_type_ != PointerActionType::PROCESS);
    index_ = index;
  }

  void set_position(const gfx::PointF& position) {
    DCHECK(pointer_action_type_ == PointerActionType::PRESS ||
           pointer_action_type_ == PointerActionType::MOVE);
    position_ = position;
  }

  PointerActionType pointer_action_type() const { return pointer_action_type_; }

  int index() const {
    DCHECK(pointer_action_type_ != PointerActionType::PROCESS);
    return index_;
  }

  gfx::PointF position() const {
    DCHECK(pointer_action_type_ == PointerActionType::PRESS ||
           pointer_action_type_ == PointerActionType::MOVE);
    return position_;
  }

 private:
  friend struct IPC::ParamTraits<content::SyntheticPointerActionParams>;
  friend struct ipc_fuzzer::FuzzTraits<content::SyntheticPointerActionParams>;

  PointerActionType pointer_action_type_;
  // Pass a position value when sending a press or move action.
  gfx::PointF position_;
  // Pass an index value except if the pointer_action_type_ is PROCESS.
  int index_;
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_SYNTHETIC_POINTER_ACTION_PARAMS_H_
