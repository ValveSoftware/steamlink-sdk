// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_INPUT_MESSAGE_CONVERTER_H_
#define BLIMP_NET_INPUT_MESSAGE_CONVERTER_H_

#include <memory>

#include "base/macros.h"
#include "blimp/common/proto/ime.pb.h"
#include "blimp/net/blimp_net_export.h"
#include "ui/base/ime/text_input_type.h"

namespace blink {
class WebGestureEvent;
}

namespace blimp {

class InputMessage;

// A generic class to provide conversion utilities for protos such as :
// 1) Creating WebGestureEvents from a stream of InputMessage protos.
//    This class may be stateful to optimize the size of the serialized
//    transmission
//    data. See InputMessageConverter for the deserialize code.
// 2) Conversion between ui::TextInputType and ImeMessage proto.
class BLIMP_NET_EXPORT InputMessageConverter {
 public:
  InputMessageConverter();
  ~InputMessageConverter();

  // Process an InputMessage and create a WebGestureEvent from it.  This might
  // make use of state from previous messages.
  std::unique_ptr<blink::WebGestureEvent> ProcessMessage(
      const InputMessage& message);

  // Converts a ui::TextInputType to ImeMessage proto.
  static ImeMessage_InputType TextInputTypeToProto(ui::TextInputType type);
  static ui::TextInputType TextInputTypeFromProto(ImeMessage_InputType type);

 private:
  DISALLOW_COPY_AND_ASSIGN(InputMessageConverter);
};

}  // namespace blimp

#endif  // BLIMP_NET_INPUT_MESSAGE_CONVERTER_H_
