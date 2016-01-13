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

#ifndef Key_h
#define Key_h

#include "bindings/v8/ScriptWrappable.h"
#include "modules/crypto/NormalizeAlgorithm.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebCryptoAlgorithm.h"
#include "public/platform/WebCryptoKey.h"
#include "wtf/Forward.h"
#include "wtf/RefCounted.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class CryptoResult;
class KeyAlgorithm;

class Key : public GarbageCollectedFinalized<Key>, public ScriptWrappable {
public:
    static Key* create(const blink::WebCryptoKey& key)
    {
        return new Key(key);
    }

    ~Key();

    String type() const;
    bool extractable() const;
    KeyAlgorithm* algorithm();
    Vector<String> usages() const;

    const blink::WebCryptoKey& key() const { return m_key; }

    // If the key cannot be used with the indicated algorithm, returns false
    // and completes the CryptoResult with an error.
    bool canBeUsedForAlgorithm(const blink::WebCryptoAlgorithm&, blink::WebCryptoOperation, CryptoResult*) const;

    // On failure, these return false and complete the CryptoResult with an error.
    static bool parseFormat(const String&, blink::WebCryptoKeyFormat&, CryptoResult*);
    static bool parseUsageMask(const Vector<String>&, blink::WebCryptoKeyUsageMask&, CryptoResult*);

    void trace(Visitor*);

protected:
    explicit Key(const blink::WebCryptoKey&);

    const blink::WebCryptoKey m_key;
    Member<KeyAlgorithm> m_algorithm;
};

} // namespace WebCore

#endif
