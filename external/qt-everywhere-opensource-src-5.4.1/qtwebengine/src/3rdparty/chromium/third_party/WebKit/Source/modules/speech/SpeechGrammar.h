/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SpeechGrammar_h
#define SpeechGrammar_h

#include "bindings/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class ExecutionContext;

class SpeechGrammar FINAL : public GarbageCollectedFinalized<SpeechGrammar>, public ScriptWrappable {
public:
    static SpeechGrammar* create(); // FIXME: The spec is not clear on what the constructor should look like.
    static SpeechGrammar* create(const KURL& src, double weight);

    const KURL& src(ExecutionContext*) const { return m_src; }
    const KURL& src() const { return m_src; }
    void setSrc(ExecutionContext*, const String& src);

    double weight() const { return m_weight; }
    void setWeight(double weight) { m_weight = weight; }

    void trace(Visitor*) { }

private:
    SpeechGrammar();
    SpeechGrammar(const KURL& src, double weight);

    KURL m_src;
    double m_weight;
};

} // namespace WebCore

#endif // SpeechGrammar_h
