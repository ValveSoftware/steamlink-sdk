// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/drop_data.h"

namespace content {

DropData::DropData()
    : did_originate_from_renderer(false),
      referrer_policy(blink::WebReferrerPolicyDefault) {}

DropData::~DropData() {
}

}  // namespace content
