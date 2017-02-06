// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Transferables_h
#define Transferables_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"

namespace blink {

class DOMArrayBufferBase;
class ImageBitmap;
class OffscreenCanvas;
class MessagePort;

using ArrayBufferArray = HeapVector<Member<DOMArrayBufferBase>>;
using ImageBitmapArray = HeapVector<Member<ImageBitmap>>;
using OffscreenCanvasArray = HeapVector<Member<OffscreenCanvas>>;
using MessagePortArray = HeapVector<Member<MessagePort>>;

class CORE_EXPORT Transferables final {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(Transferables);
public:
    Transferables()
    {
    }

    ArrayBufferArray arrayBuffers;
    ImageBitmapArray imageBitmaps;
    OffscreenCanvasArray offscreenCanvases;
    MessagePortArray messagePorts;
};

// Along with extending |Transferables| to hold a new kind of transferable
// objects, serialization handling code changes are required:
//   - extend ScriptValueSerializer::copyTransferables()
//   - alter SerializedScriptValue(Factory) to do the actual transfer.

} // namespace blink

#endif // Transferables_h
