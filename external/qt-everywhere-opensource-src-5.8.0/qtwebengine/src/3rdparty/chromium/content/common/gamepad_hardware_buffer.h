// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GAMEPAD_HARDWARE_BUFFER_H_
#define CONTENT_COMMON_GAMEPAD_HARDWARE_BUFFER_H_

#include "content/common/one_writer_seqlock.h"
#include "third_party/WebKit/public/platform/WebGamepads.h"

namespace content {

/*

This structure is stored in shared memory that's shared between the browser
which does the hardware polling, and the various consumers of the gamepad
state (renderers and NaCl plugins). The performance characteristics are that
we want low latency (so would like to avoid explicit communication via IPC
between producer and consumer) and relatively large data size.

Writer and reader operate on the same buffer assuming contention is low, and
contention is detected by using the associated SeqLock.

*/

struct GamepadHardwareBuffer {
  // FIXME: Use the generic SharedMemorySeqLockBuffer<blink::WebGamepads>.
  OneWriterSeqLock sequence;
  blink::WebGamepads buffer;
};

}  // namespace content

#endif  // CONTENT_COMMON_GAMEPAD_HARDWARE_BUFFER_H_
