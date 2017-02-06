// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/begin_frame_args.h"

#include "base/trace_event/trace_event_argument.h"
#include "cc/proto/base_conversions.h"
#include "cc/proto/begin_main_frame_and_commit_state.pb.h"

namespace cc {

const char* BeginFrameArgs::TypeToString(BeginFrameArgsType type) {
  switch (type) {
    case BeginFrameArgs::INVALID:
      return "INVALID";
    case BeginFrameArgs::NORMAL:
      return "NORMAL";
    case BeginFrameArgs::MISSED:
      return "MISSED";
    case BeginFrameArgs::BEGIN_FRAME_ARGS_TYPE_MAX:
      return "BEGIN_FRAME_ARGS_TYPE_MAX";
  }
  NOTREACHED();
  return "???";
}

void BeginFrameArgs::BeginFrameArgsTypeToProtobuf(
    proto::BeginFrameArgs* proto) const {
  switch (type) {
    case BeginFrameArgs::INVALID:
      proto->set_type(proto::BeginFrameArgs::INVALID);
      return;
    case BeginFrameArgs::NORMAL:
      proto->set_type(proto::BeginFrameArgs::NORMAL);
      return;
    case BeginFrameArgs::MISSED:
      proto->set_type(proto::BeginFrameArgs::MISSED);
      return;
    case BeginFrameArgs::BEGIN_FRAME_ARGS_TYPE_MAX:
      proto->set_type(proto::BeginFrameArgs::BEGIN_FRAME_ARGS_TYPE_MAX);
      return;
  }
  NOTREACHED();
}

void BeginFrameArgs::BeginFrameArgsTypeFromProtobuf(
    const proto::BeginFrameArgs& proto) {
  switch (proto.type()) {
    case proto::BeginFrameArgs::INVALID:
      type = BeginFrameArgs::INVALID;
      return;
    case proto::BeginFrameArgs::NORMAL:
      type = BeginFrameArgs::NORMAL;
      return;
    case proto::BeginFrameArgs::MISSED:
      type = BeginFrameArgs::MISSED;
      return;
    case proto::BeginFrameArgs::BEGIN_FRAME_ARGS_TYPE_MAX:
      type = BeginFrameArgs::BEGIN_FRAME_ARGS_TYPE_MAX;
      return;
  }
  NOTREACHED();
}

BeginFrameArgs::BeginFrameArgs()
    : frame_time(base::TimeTicks()),
      deadline(base::TimeTicks()),
      interval(base::TimeDelta::FromMicroseconds(-1)),
      type(BeginFrameArgs::INVALID),
      on_critical_path(true) {
}

BeginFrameArgs::BeginFrameArgs(base::TimeTicks frame_time,
                               base::TimeTicks deadline,
                               base::TimeDelta interval,
                               BeginFrameArgs::BeginFrameArgsType type)
    : frame_time(frame_time),
      deadline(deadline),
      interval(interval),
      type(type),
      on_critical_path(true) {
}

BeginFrameArgs BeginFrameArgs::Create(BeginFrameArgs::CreationLocation location,
                                      base::TimeTicks frame_time,
                                      base::TimeTicks deadline,
                                      base::TimeDelta interval,
                                      BeginFrameArgs::BeginFrameArgsType type) {
  DCHECK_NE(type, BeginFrameArgs::INVALID);
  DCHECK_NE(type, BeginFrameArgs::BEGIN_FRAME_ARGS_TYPE_MAX);
#ifdef NDEBUG
  return BeginFrameArgs(frame_time, deadline, interval, type);
#else
  BeginFrameArgs args = BeginFrameArgs(frame_time, deadline, interval, type);
  args.created_from = location;
  return args;
#endif
}

std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
BeginFrameArgs::AsValue() const {
  std::unique_ptr<base::trace_event::TracedValue> state(
      new base::trace_event::TracedValue());
  AsValueInto(state.get());
  return std::move(state);
}

void BeginFrameArgs::AsValueInto(base::trace_event::TracedValue* state) const {
  state->SetString("type", "BeginFrameArgs");
  state->SetString("subtype", TypeToString(type));
  state->SetDouble("frame_time_us", frame_time.ToInternalValue());
  state->SetDouble("deadline_us", deadline.ToInternalValue());
  state->SetDouble("interval_us", interval.InMicroseconds());
#ifndef NDEBUG
  state->SetString("created_from", created_from.ToString());
#endif
  state->SetBoolean("on_critical_path", on_critical_path);
}

void BeginFrameArgs::ToProtobuf(proto::BeginFrameArgs* proto) const {
  proto->set_frame_time(TimeTicksToProto(frame_time));
  proto->set_deadline(TimeTicksToProto(deadline));
  proto->set_interval(interval.ToInternalValue());
  BeginFrameArgsTypeToProtobuf(proto);
  proto->set_on_critical_path(on_critical_path);
}

void BeginFrameArgs::FromProtobuf(const proto::BeginFrameArgs& proto) {
  frame_time = ProtoToTimeTicks(proto.frame_time());
  deadline = ProtoToTimeTicks(proto.deadline());
  interval = base::TimeDelta::FromInternalValue(proto.interval());
  BeginFrameArgsTypeFromProtobuf(proto);
  on_critical_path = proto.on_critical_path();
}

// This is a hard-coded deadline adjustment that assumes 60Hz, to be used in
// cases where a good estimated draw time is not known. Using 1/3 of the vsync
// as the default adjustment gives the Browser the last 1/3 of a frame to
// produce output, the Renderer Impl thread the middle 1/3 of a frame to produce
// ouput, and the Renderer Main thread the first 1/3 of a frame to produce
// output.
base::TimeDelta BeginFrameArgs::DefaultEstimatedParentDrawTime() {
  return base::TimeDelta::FromMicroseconds(16666 / 3);
}

base::TimeDelta BeginFrameArgs::DefaultInterval() {
  return base::TimeDelta::FromMicroseconds(16666);
}

}  // namespace cc
