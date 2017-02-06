/*
 * Copyright (C) 2009, 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SerializedScriptValue_h
#define SerializedScriptValue_h

#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/Transferables.h"
#include "core/CoreExport.h"
#include "wtf/HashMap.h"
#include "wtf/ThreadSafeRefCounted.h"
#include "wtf/typed_arrays/ArrayBufferContents.h"
#include <memory>
#include <v8.h>

namespace blink {

class BlobDataHandle;
class Transferables;
class ExceptionState;
class StaticBitmapImage;
class WebBlobInfo;

typedef HashMap<String, RefPtr<BlobDataHandle>> BlobDataHandleMap;
typedef Vector<WebBlobInfo> WebBlobInfoArray;

class CORE_EXPORT SerializedScriptValue : public ThreadSafeRefCounted<SerializedScriptValue> {
public:
    using ArrayBufferContentsArray = Vector<WTF::ArrayBufferContents, 1>;
    using ImageBitmapContentsArray = Vector<RefPtr<StaticBitmapImage>, 1>;

    // Increment this for each incompatible change to the wire format.
    // Version 2: Added StringUCharTag for UChar v8 strings.
    // Version 3: Switched to using uuids as blob data identifiers.
    // Version 4: Extended File serialization to be complete.
    // Version 5: Added CryptoKeyTag for Key objects.
    // Version 6: Added indexed serialization for File, Blob, and FileList.
    // Version 7: Extended File serialization with user visibility.
    // Version 8: File.lastModified in milliseconds (seconds-based in earlier versions.)
    // Version 9: Added Map and Set support.
    static const uint32_t wireFormatVersion = 9;

    // VarInt encoding constants.
    static const int varIntShift = 7;
    static const int varIntMask = (1 << varIntShift) - 1;

    static PassRefPtr<SerializedScriptValue> serialize(v8::Isolate*, v8::Local<v8::Value>, Transferables*, WebBlobInfoArray*, ExceptionState&);
    static PassRefPtr<SerializedScriptValue> serialize(const String&);
    static PassRefPtr<SerializedScriptValue> serializeAndSwallowExceptions(v8::Isolate*, v8::Local<v8::Value>);

    static PassRefPtr<SerializedScriptValue> create();
    static PassRefPtr<SerializedScriptValue> create(const String&);
    static PassRefPtr<SerializedScriptValue> create(const char* data, size_t length);

    virtual ~SerializedScriptValue();

    static PassRefPtr<SerializedScriptValue> nullValue();

    String toWireString() const { return m_data; }
    void toWireBytes(Vector<char>&) const;

    // Deserializes the value (in the current context). Returns a null value in
    // case of failure.
    v8::Local<v8::Value> deserialize(MessagePortArray* = 0);
    v8::Local<v8::Value> deserialize(v8::Isolate*, MessagePortArray* = 0, const WebBlobInfoArray* = 0);

    // Helper function which pulls the values out of a JS sequence and into a MessagePortArray.
    // Also validates the elements per sections 4.1.13 and 4.1.15 of the WebIDL spec and section 8.3.3
    // of the HTML5 spec and generates exceptions as appropriate.
    // Returns true if the array was filled, or false if the passed value was not of an appropriate type.
    static bool extractTransferables(v8::Isolate*, v8::Local<v8::Value>, int, Transferables&, ExceptionState&);

    // Informs the V8 about external memory allocated and owned by this object. Large values should contribute
    // to GC counters to eventually trigger a GC, otherwise flood of postMessage() can cause OOM.
    // Ok to invoke multiple times (only adds memory once).
    // The memory registration is revoked automatically in destructor.
    void registerMemoryAllocatedWithCurrentScriptContext();

    // Returns true if the value contains a transferable ArrayBuffer.
    bool containsTransferableArrayBuffer() const;

    String& data() { return m_data; }
    BlobDataHandleMap& blobDataHandles() { return m_blobDataHandles; }
    ArrayBufferContentsArray* getArrayBufferContentsArray() { return m_arrayBufferContentsArray.get(); }
    ImageBitmapContentsArray* getImageBitmapContentsArray() { return m_imageBitmapContentsArray.get(); }

private:
    friend class ScriptValueSerializer;

    enum StringDataMode {
        StringValue,
        WireData
    };

    SerializedScriptValue();
    explicit SerializedScriptValue(const String& wireData);

    void setData(const String& data) { m_data = data; }
    void transferArrayBuffers(v8::Isolate*, const ArrayBufferArray&, ExceptionState&);
    void transferImageBitmaps(v8::Isolate*, const ImageBitmapArray&, ExceptionState&);
    void transferOffscreenCanvas(v8::Isolate*, const OffscreenCanvasArray&, ExceptionState&);

    String m_data;
    std::unique_ptr<ArrayBufferContentsArray> m_arrayBufferContentsArray;
    std::unique_ptr<ImageBitmapContentsArray> m_imageBitmapContentsArray;
    BlobDataHandleMap m_blobDataHandles;
    intptr_t m_externallyAllocatedMemory;
};

} // namespace blink

#endif // SerializedScriptValue_h
