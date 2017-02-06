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

#include "modules/mediastream/MediaErrorState.h"

#include "core/dom/ExceptionCode.h"
#include "modules/mediastream/NavigatorUserMediaError.h"

namespace blink {

MediaErrorState::MediaErrorState()
    : m_errorType(NoError)
    , m_code(0)
{
}

void MediaErrorState::throwTypeError(const String& message)
{
    m_errorType = TypeError;
    m_message = message;
}

void MediaErrorState::throwDOMException(const ExceptionCode& code, const String& message)
{
    m_errorType = DOMException;
    m_code = code;
    m_message = message;
}

void MediaErrorState::throwConstraintError(const String& message, const String& constraint)
{
    m_errorType = ConstraintError;
    m_message = message;
    m_constraint = constraint;
}

void MediaErrorState::reset()
{
    m_errorType = NoError;
}

bool MediaErrorState::hadException()
{
    return m_errorType != NoError;
}

bool MediaErrorState::canGenerateException()
{
    return m_errorType == TypeError || m_errorType == DOMException;
}

void MediaErrorState::raiseException(ExceptionState& target)
{
    switch (m_errorType) {
    case NoError:
        NOTREACHED();
        break;
    case TypeError:
        target.throwTypeError(m_message);
        break;
    case DOMException:
        target.throwDOMException(m_code, m_message);
        break;
    case ConstraintError:
        // This is for the cases where we can't pass back a
        // NavigatorUserMediaError.
        // So far, we have this in the constructor of RTCPeerConnection,
        // which is due to be deprecated.
        // TODO(hta): Remove this code. https://crbug.com/576581
        target.throwDOMException(NotSupportedError, "Unsatisfiable constraint " + m_constraint);
        break;
    default:
        NOTREACHED();
    }
}

String MediaErrorState::getErrorMessage()
{
    switch (m_errorType) {
    case NoError:
        NOTREACHED();
        break;
    case TypeError:
    case DOMException:
        return m_message;
    case ConstraintError:
        // This is for the cases where we can't pass back a
        // NavigatorUserMediaError.
        // So far, we have this in the constructor of RTCPeerConnection,
        // which is due to be deprecated.
        // TODO(hta): Remove this code. https://crbug.com/576581
        return "Unsatisfiable constraint " + m_constraint;
    default:
        NOTREACHED();
    }

    return String();
}

NavigatorUserMediaError* MediaErrorState::createError()
{
    DCHECK(m_errorType == ConstraintError);
    return NavigatorUserMediaError::create(NavigatorUserMediaError::NameConstraintNotSatisfied, m_message, m_constraint);
}


} // namespace blink
