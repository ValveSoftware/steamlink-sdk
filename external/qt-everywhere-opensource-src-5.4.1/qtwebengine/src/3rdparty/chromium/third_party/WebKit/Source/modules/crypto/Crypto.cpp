/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Google, Inc. ("Google") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "config.h"
#include "modules/crypto/Crypto.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "wtf/ArrayBufferView.h"
#include "wtf/CryptographicallyRandomNumber.h"

namespace WebCore {

namespace {

bool isIntegerArray(ArrayBufferView* array)
{
    ArrayBufferView::ViewType type = array->type();
    return type == ArrayBufferView::TypeInt8
        || type == ArrayBufferView::TypeUint8
        || type == ArrayBufferView::TypeUint8Clamped
        || type == ArrayBufferView::TypeInt16
        || type == ArrayBufferView::TypeUint16
        || type == ArrayBufferView::TypeInt32
        || type == ArrayBufferView::TypeUint32;
}

}

Crypto::Crypto()
{
    ScriptWrappable::init(this);
}

void Crypto::getRandomValues(ArrayBufferView* array, ExceptionState& exceptionState)
{
    if (!array) {
        exceptionState.throwDOMException(TypeMismatchError, "The provided ArrayBufferView is null.");
        return;
    }
    if (!isIntegerArray(array)) {
        exceptionState.throwDOMException(TypeMismatchError, String::format("The provided ArrayBufferView is of type '%s', which is not an integer array type.", array->typeName()));
        return;
    }
    if (array->byteLength() > 65536) {
        exceptionState.throwDOMException(QuotaExceededError, String::format("The ArrayBufferView's byte length (%u) exceeds the number of bytes of entropy available via this API (65536).", array->byteLength()));
        return;
    }
    cryptographicallyRandomValues(array->baseAddress(), array->byteLength());
}

SubtleCrypto* Crypto::subtle()
{
    if (!m_subtleCrypto)
        m_subtleCrypto = SubtleCrypto::create();
    return m_subtleCrypto.get();
}

void Crypto::trace(Visitor* visitor)
{
    visitor->trace(m_subtleCrypto);
}

}
