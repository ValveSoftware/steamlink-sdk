// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GAMEPAD_PARAM_TRAITS_H_
#define CONTENT_COMMON_GAMEPAD_PARAM_TRAITS_H_

#include <string>

#include "ipc/ipc_param_traits.h"

class PickleIterator;

namespace blink { class WebGamepad; }

namespace IPC {

class Message;

template <>
struct ParamTraits<blink::WebGamepad> {
  typedef blink::WebGamepad param_type;
  static void Write(Message* m, const blink::WebGamepad& p);
  static bool Read(const Message* m,
                   PickleIterator* iter,
                   blink::WebGamepad* p);
  static void Log(const blink::WebGamepad& p, std::string* l);
};

} // namespace IPC

#endif
