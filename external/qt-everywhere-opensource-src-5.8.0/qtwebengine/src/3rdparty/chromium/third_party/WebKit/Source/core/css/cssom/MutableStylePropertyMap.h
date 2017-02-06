// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MutableStylePropertyMap_h
#define MutableStylePropertyMap_h

#include "core/css/cssom/StylePropertyMap.h"

namespace blink {

class CORE_EXPORT MutableStylePropertyMap : public StylePropertyMap {
    WTF_MAKE_NONCOPYABLE(MutableStylePropertyMap);
protected:
    MutableStylePropertyMap() = default;
};

} // namespace blink

#endif
