// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaQueryBlockWatcher_h
#define MediaQueryBlockWatcher_h

#include "core/CoreExport.h"
#include "wtf/Allocator.h"

namespace blink {

class CSSParserToken;

class CORE_EXPORT MediaQueryBlockWatcher {
    STACK_ALLOCATED();
public:
    MediaQueryBlockWatcher();
    void handleToken(const CSSParserToken&);
    unsigned blockLevel() const { return m_blockLevel; }

private:
    unsigned m_blockLevel;
};

} // namespace blink

#endif // MediaQueryBlockWatcher_h
