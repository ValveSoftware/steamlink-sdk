// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EMBEDDER_CHANNEL_INIT_H_
#define MOJO_EMBEDDER_CHANNEL_INIT_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/system/system_impl_export.h"

namespace base {
class MessageLoopProxy;
class TaskRunner;
}

namespace mojo {
namespace embedder {
struct ChannelInfo;
}

namespace embedder {

// ChannelInit handle creation (and destruction) of the mojo channel. It is
// expected that this class is created and destroyed on the main thread.
class MOJO_SYSTEM_IMPL_EXPORT ChannelInit {
 public:
  ChannelInit();
  ~ChannelInit();

  // Initializes the channel. This takes ownership of |file|. Returns the
  // primordial MessagePipe for the channel.
  mojo::ScopedMessagePipeHandle Init(
      base::PlatformFile file,
      scoped_refptr<base::TaskRunner> io_thread_task_runner);

 private:
  // Invoked on the main thread once the channel has been established.
  static void OnCreatedChannel(
      base::WeakPtr<ChannelInit> host,
      scoped_refptr<base::TaskRunner> io_thread,
      embedder::ChannelInfo* channel);

  scoped_refptr<base::TaskRunner> io_thread_task_runner_;

  // If non-null the channel has been established.
  embedder::ChannelInfo* channel_info_;

  base::WeakPtrFactory<ChannelInit> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChannelInit);
};

}  // namespace embedder
}  // namespace mojo

#endif  // MOJO_EMBEDDER_CHANNEL_INIT_H_
