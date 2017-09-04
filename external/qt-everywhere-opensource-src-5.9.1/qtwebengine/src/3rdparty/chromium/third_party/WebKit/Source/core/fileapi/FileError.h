/*
 * Copyright (C) 2010 Google Inc.  All rights reserved.
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

#ifndef FileError_h
#define FileError_h

#include "core/CoreExport.h"

namespace blink {

class DOMException;
class ExceptionState;

namespace FileError {

enum ErrorCode {
  kOK = 0,
  kNotFoundErr = 1,
  kSecurityErr = 2,
  kAbortErr = 3,
  kNotReadableErr = 4,
  kEncodingErr = 5,
  kNoModificationAllowedErr = 6,
  kInvalidStateErr = 7,
  kSyntaxErr = 8,
  kInvalidModificationErr = 9,
  kQuotaExceededErr = 10,
  kTypeMismatchErr = 11,
  kPathExistsErr = 12,
};

CORE_EXPORT extern const char abortErrorMessage[];
CORE_EXPORT extern const char encodingErrorMessage[];
CORE_EXPORT extern const char invalidStateErrorMessage[];
CORE_EXPORT extern const char noModificationAllowedErrorMessage[];
CORE_EXPORT extern const char notFoundErrorMessage[];
CORE_EXPORT extern const char notReadableErrorMessage[];
CORE_EXPORT extern const char pathExistsErrorMessage[];
CORE_EXPORT extern const char quotaExceededErrorMessage[];
CORE_EXPORT extern const char securityErrorMessage[];
CORE_EXPORT extern const char syntaxErrorMessage[];
CORE_EXPORT extern const char typeMismatchErrorMessage[];

CORE_EXPORT void throwDOMException(ExceptionState&, ErrorCode);
CORE_EXPORT DOMException* createDOMException(ErrorCode);

}  // namespace FileError

}  // namespace blink

#endif  // FileError_h
