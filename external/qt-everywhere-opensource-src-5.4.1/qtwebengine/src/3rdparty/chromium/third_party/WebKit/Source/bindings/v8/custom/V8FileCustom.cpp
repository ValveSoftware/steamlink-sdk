/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "config.h"
#include "bindings/core/v8/V8File.h"

#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/custom/V8BlobCustomHelpers.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace WebCore {

void V8File::constructorCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "File", info.Holder(), info.GetIsolate());

    if (!RuntimeEnabledFeatures::fileConstructorEnabled()) {
        exceptionState.throwTypeError("Illegal constructor");
        exceptionState.throwIfNeeded();
        return;
    }

    if (info.Length() < 2) {
        exceptionState.throwTypeError(ExceptionMessages::notEnoughArguments(2, info.Length()));
        exceptionState.throwIfNeeded();
        return;
    }

    uint32_t length = 0;
    if (info[0]->IsArray()) {
        length = v8::Local<v8::Array>::Cast(info[0])->Length();
    } else {
        const int sequenceArgumentIndex = 0;
        if (toV8Sequence(info[sequenceArgumentIndex], length, info.GetIsolate()).IsEmpty()) {
            exceptionState.throwTypeError(ExceptionMessages::notAnArrayTypeArgumentOrValue(sequenceArgumentIndex + 1));
            exceptionState.throwIfNeeded();
            return;
        }
    }

    TOSTRING_VOID(V8StringResource<>, fileName, info[1]);

    V8BlobCustomHelpers::ParsedProperties properties(true);
    if (info.Length() > 2) {
        if (!info[2]->IsObject()) {
            exceptionState.throwTypeError("The 3rd argument is not of type Object.");
            exceptionState.throwIfNeeded();
            return;
        }

        if (!properties.parseBlobPropertyBag(info[2], "File", exceptionState, info.GetIsolate())) {
            exceptionState.throwIfNeeded();
            return;
        }
    } else {
        properties.setDefaultLastModified();
    }

    OwnPtr<BlobData> blobData = BlobData::create();
    blobData->setContentType(properties.contentType());
    v8::Local<v8::Object> blobParts = v8::Local<v8::Object>::Cast(info[0]);
    if (!V8BlobCustomHelpers::processBlobParts(blobParts, length, properties.normalizeLineEndingsToNative(), *blobData, info.GetIsolate()))
        return;

    long long fileSize = blobData->length();
    RefPtrWillBeRawPtr<File> file = File::create(fileName, properties.lastModified(), BlobDataHandle::create(blobData.release(), fileSize));
    v8SetReturnValue(info, file.release());
}

void V8File::lastModifiedDateAttributeGetterCustom(const v8::PropertyCallbackInfo<v8::Value>& info)
{
    // The auto-generated getters return null when the method in the underlying
    // implementation returns NaN. The File API says we should return the
    // current time when the last modification time is unknown.
    // Section 7.2 of the File API spec. http://dev.w3.org/2006/webapi/FileAPI/

    File* file = V8File::toNative(info.Holder());
    double lastModified = file->lastModifiedDate();
    if (!isValidFileTime(lastModified))
        lastModified = currentTimeMS();

    // lastModifiedDate returns a Date instance.
    // http://www.w3.org/TR/FileAPI/#file-attrs
    v8SetReturnValue(info, v8::Date::New(info.GetIsolate(), lastModified));
}

void V8File::lastModifiedAttributeGetterCustom(const v8::PropertyCallbackInfo<v8::Value>& info)
{
    File* file = V8File::toNative(info.Holder());
    double lastModified = file->lastModifiedDate();
    if (!isValidFileTime(lastModified))
        lastModified = currentTimeMS();

    // lastModified returns a number, not a Date instance.
    // http://dev.w3.org/2006/webapi/FileAPI/#file-attrs
    v8SetReturnValue(info, floor(lastModified));
}

} // namespace WebCore
