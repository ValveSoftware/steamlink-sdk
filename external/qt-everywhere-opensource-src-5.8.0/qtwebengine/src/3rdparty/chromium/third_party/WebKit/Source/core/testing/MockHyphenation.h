// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MockHyphenation_h
#define MockHyphenation_h

#include "platform/text/Hyphenation.h"

namespace blink {

class MockHyphenation : public Hyphenation {
public:
    size_t lastHyphenLocation(const StringView&, size_t beforeIndex) const override;
};

} // namespace blink

#endif // MockHyphenation_h
