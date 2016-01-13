// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/layer_updater.h"

#include "cc/resources/prioritized_resource.h"

namespace cc {

LayerUpdater::Resource::Resource(scoped_ptr<PrioritizedResource> texture)
    : texture_(texture.Pass()) {}

LayerUpdater::Resource::~Resource() {}

}  // namespace cc
