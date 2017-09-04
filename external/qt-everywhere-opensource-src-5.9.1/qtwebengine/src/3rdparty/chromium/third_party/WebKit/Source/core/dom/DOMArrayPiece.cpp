// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/DOMArrayPiece.h"

#include "bindings/core/v8/ArrayBufferOrArrayBufferView.h"

namespace blink {

DOMArrayPiece::DOMArrayPiece(
    const ArrayBufferOrArrayBufferView& arrayBufferOrView,
    InitWithUnionOption option) {
  if (arrayBufferOrView.isArrayBuffer()) {
    DOMArrayBuffer* arrayBuffer = arrayBufferOrView.getAsArrayBuffer();
    initWithData(arrayBuffer->data(), arrayBuffer->byteLength());
  } else if (arrayBufferOrView.isArrayBufferView()) {
    DOMArrayBufferView* arrayBufferView =
        arrayBufferOrView.getAsArrayBufferView();
    initWithData(arrayBufferView->baseAddress(), arrayBufferView->byteLength());
  } else if (arrayBufferOrView.isNull() &&
             option == AllowNullPointToNullWithZeroSize) {
    initWithData(nullptr, 0);
  }  // Otherwise, leave the obejct as null.
}

}  // namespace blink
