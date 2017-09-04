// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TypedFlexibleArrayBufferView_h
#define TypedFlexibleArrayBufferView_h

#include "core/CoreExport.h"
#include "core/dom/FlexibleArrayBufferView.h"
#include "wtf/Noncopyable.h"

namespace blink {

template <typename WTFTypedArray>
class CORE_TEMPLATE_CLASS_EXPORT TypedFlexibleArrayBufferView final
    : public FlexibleArrayBufferView {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(TypedFlexibleArrayBufferView);

 public:
  using ValueType = typename WTFTypedArray::ValueType;

  TypedFlexibleArrayBufferView() : FlexibleArrayBufferView() {}

  ValueType* dataMaybeOnStack() const {
    return static_cast<ValueType*>(baseAddressMaybeOnStack());
  }

  unsigned length() const {
    DCHECK_EQ(byteLength() % sizeof(ValueType), 0u);
    return byteLength() / sizeof(ValueType);
  }
};

using FlexibleFloat32ArrayView =
    TypedFlexibleArrayBufferView<WTF::Float32Array>;
using FlexibleInt32ArrayView = TypedFlexibleArrayBufferView<WTF::Int32Array>;
using FlexibleUint32ArrayView = TypedFlexibleArrayBufferView<WTF::Uint32Array>;

}  // namespace blink

#endif  // TypedFlexibleArrayBufferView_h
