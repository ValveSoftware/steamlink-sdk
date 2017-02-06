/*
 * Copyright (C) 2015 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#ifndef MediaErrorState_h
#define MediaErrorState_h

#include "bindings/core/v8/ExceptionState.h"

namespace blink {

class NavigatorUserMediaError;

// A class that is able to be used like ExceptionState for carrying
// information about an error up the stack, but it is up to the higher
// level code whether it produces a DOMException or a NavigatorUserMediaError.
class MediaErrorState {
public:
    MediaErrorState();
    void throwTypeError(const String& message);
    void throwDOMException(const ExceptionCode&, const String& message);
    void throwConstraintError(const String& message, const String& constraint);
    void reset();

    bool hadException();
    bool canGenerateException();
    void raiseException(ExceptionState&);
    String getErrorMessage();
    NavigatorUserMediaError* createError();
private:
    enum ErrorType {
        NoError,
        TypeError,
        DOMException,
        ConstraintError
    };
    ErrorType m_errorType;
    String m_name;
    ExceptionCode m_code;
    String m_message;
    String m_constraint;
};

} // namespace blink

#endif // MediaErrorState_h
