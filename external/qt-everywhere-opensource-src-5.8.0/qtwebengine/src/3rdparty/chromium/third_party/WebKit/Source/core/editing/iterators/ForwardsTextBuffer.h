// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ForwardsTextBuffer_h
#define ForwardsTextBuffer_h

#include "core/editing/iterators/TextBufferBase.h"

namespace blink {

class CORE_EXPORT ForwardsTextBuffer final : public TextBufferBase {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(ForwardsTextBuffer);
public:
    ForwardsTextBuffer() {}
    const UChar* data() const override;

private:
    UChar* calcDestination(size_t length) override;
};

} // namespace blink

#endif // TextBuffer_h
