// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/sync_dispatcher.h"

#include <stdlib.h>

#include "mojo/public/cpp/bindings/message.h"

namespace mojo {

bool WaitForMessageAndDispatch(MessagePipeHandle handle,
                               MessageReceiver* receiver) {
  while (true) {
    bool result;
    MojoResult rv = ReadAndDispatchMessage(handle, receiver, &result);
    if (rv == MOJO_RESULT_OK)
      return result;
    if (rv == MOJO_RESULT_SHOULD_WAIT)
      rv = Wait(handle, MOJO_HANDLE_SIGNAL_READABLE, MOJO_DEADLINE_INDEFINITE);
    if (rv != MOJO_RESULT_OK)
      return false;
  }
}

}  // namespace mojo
