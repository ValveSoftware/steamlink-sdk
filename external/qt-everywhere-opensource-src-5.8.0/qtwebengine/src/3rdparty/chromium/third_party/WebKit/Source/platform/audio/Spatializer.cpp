// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/audio/Spatializer.h"
#include "platform/audio/StereoPanner.h"
#include "wtf/PtrUtil.h"
#include <memory>

namespace blink {

std::unique_ptr<Spatializer> Spatializer::create(PanningModel model, float sampleRate)
{
    switch (model) {
    case PanningModelEqualPower:
        return wrapUnique(new StereoPanner(sampleRate));
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }
}

Spatializer::~Spatializer()
{
}

} // namespace blink

