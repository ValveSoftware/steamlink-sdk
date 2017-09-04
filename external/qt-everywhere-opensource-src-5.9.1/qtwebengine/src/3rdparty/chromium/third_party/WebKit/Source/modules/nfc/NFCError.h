// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NFCError_h
#define NFCError_h

#include "device/nfc/nfc.mojom-blink.h"
#include "wtf/Allocator.h"

namespace blink {

class DOMException;
class ScriptPromiseResolver;

class NFCError {
  STATIC_ONLY(NFCError);

 public:
  // Required by CallbackPromiseAdapter
  using WebType = const device::nfc::mojom::blink::NFCErrorType&;
  static DOMException* take(ScriptPromiseResolver*,
                            const device::nfc::mojom::blink::NFCErrorType&);
};

}  // namespace blink

#endif  // NFCError_h
