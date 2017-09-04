// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/resource/LinkPreloadResourceClients.h"

#include "core/loader/LinkLoader.h"

namespace blink {

void LinkPreloadResourceClient::triggerEvents(const Resource* resource) {
  if (m_loader)
    m_loader->triggerEvents(resource);
}
}
