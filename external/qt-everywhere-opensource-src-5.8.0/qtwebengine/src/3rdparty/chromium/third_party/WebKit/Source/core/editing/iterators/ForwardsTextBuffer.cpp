// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/iterators/ForwardsTextBuffer.h"

namespace blink {

const UChar* ForwardsTextBuffer::data() const
{
    return bufferBegin();
}

UChar* ForwardsTextBuffer::calcDestination(size_t length)
{
    DCHECK_LE(size() + length, capacity());
    return bufferBegin() + size();
}

} // namespace blink
