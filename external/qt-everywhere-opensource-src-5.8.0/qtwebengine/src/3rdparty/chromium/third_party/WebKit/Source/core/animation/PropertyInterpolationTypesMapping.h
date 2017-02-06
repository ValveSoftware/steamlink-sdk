// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PropertyInterpolationTypesMapping_h
#define PropertyInterpolationTypesMapping_h

#include "wtf/Vector.h"
#include <memory>

namespace blink {

class InterpolationType;
class PropertyHandle;

using InterpolationTypes = Vector<std::unique_ptr<const InterpolationType>>;

namespace PropertyInterpolationTypesMapping {

const InterpolationTypes& get(const PropertyHandle&);

};

} // namespace blink

#endif // PropertyInterpolationTypesMapping_h
