// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/app/mojo/mojo_init.h"

#include "base/logging.h"
#include "mojo/embedder/embedder.h"
#include "mojo/service_manager/service_manager.h"

namespace content {

void InitializeMojo() {
  mojo::embedder::Init();
  mojo::ServiceManager::GetInstance();
}

}  // namespace content
