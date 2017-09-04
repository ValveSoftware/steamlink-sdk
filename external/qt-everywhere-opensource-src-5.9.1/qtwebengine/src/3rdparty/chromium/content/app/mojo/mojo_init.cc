// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/mojo/mojo_init.h"

#include <memory>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc_channel.h"
#include "mojo/edk/embedder/embedder.h"

namespace content {

namespace {

class MojoInitializer {
 public:
  MojoInitializer() {
    mojo::edk::SetMaxMessageSize(IPC::Channel::kMaximumMessageSize);
    mojo::edk::Init();
  }
};

base::LazyInstance<MojoInitializer>::Leaky mojo_initializer;

}  //  namespace

void InitializeMojo() {
  mojo_initializer.Get();
}

}  // namespace content
