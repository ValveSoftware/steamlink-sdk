// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FullyClippedStateStack_h
#define FullyClippedStateStack_h

#include "core/editing/EditingStrategy.h"
#include "core/editing/iterators/BitStack.h"
#include "wtf/Allocator.h"

namespace blink {

template<typename Strategy>
class CORE_TEMPLATE_CLASS_EXPORT FullyClippedStateStackAlgorithm final : public BitStack {
    STACK_ALLOCATED();
public:
    FullyClippedStateStackAlgorithm();
    ~FullyClippedStateStackAlgorithm();

    void pushFullyClippedState(Node*);
    void setUpFullyClippedStack(Node*);
};

extern template class CORE_EXTERN_TEMPLATE_EXPORT FullyClippedStateStackAlgorithm<EditingStrategy>;
extern template class CORE_EXTERN_TEMPLATE_EXPORT FullyClippedStateStackAlgorithm<EditingInFlatTreeStrategy>;

using FullyClippedStateStack = FullyClippedStateStackAlgorithm<EditingStrategy>;

} // namespace blink

#endif // FullyClippedStateStack_h
