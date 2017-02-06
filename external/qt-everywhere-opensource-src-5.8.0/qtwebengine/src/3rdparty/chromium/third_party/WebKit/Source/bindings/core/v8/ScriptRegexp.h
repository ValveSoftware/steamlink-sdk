/*
 * Copyright (C) 2003, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ScriptRegexp_h
#define ScriptRegexp_h

#include "bindings/core/v8/ScopedPersistent.h"
#include "core/CoreExport.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/WTFString.h"
#include <v8.h>

namespace blink {

enum MultilineMode {
    MultilineDisabled,
    MultilineEnabled
};

class CORE_EXPORT ScriptRegexp {
    USING_FAST_MALLOC(ScriptRegexp); WTF_MAKE_NONCOPYABLE(ScriptRegexp);
public:
    enum CharacterMode {
        BMP, // NOLINT
        UTF16, // NOLINT
    };

    ScriptRegexp(const String&, TextCaseSensitivity, MultilineMode = MultilineDisabled, CharacterMode = BMP);

    int match(const String&, int startFrom = 0, int* matchLength = 0) const;

    bool isValid() const { return !m_regex.isEmpty(); }
    // exceptionMessage is available only if !isValid().
    String exceptionMessage() const { return m_exceptionMessage; }

private:
    ScopedPersistent<v8::RegExp> m_regex;
    String m_exceptionMessage;
};

} // namespace blink

#endif // ScriptRegexp_h
