// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/feature/geolocation/mock_blimp_location_provider_delegate.h"

namespace blimp {
namespace engine {

MockBlimpLocationProviderDelegate::MockBlimpLocationProviderDelegate()
    : weak_factory_(this) {}
MockBlimpLocationProviderDelegate::~MockBlimpLocationProviderDelegate() {}

}  // namespace engine
}  // namespace blimp
