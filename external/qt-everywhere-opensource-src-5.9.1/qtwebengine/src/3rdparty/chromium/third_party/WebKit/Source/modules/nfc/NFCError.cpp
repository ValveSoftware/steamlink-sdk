// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/nfc/NFCError.h"

#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"

using device::nfc::mojom::blink::NFCErrorType;

namespace blink {

DOMException* NFCError::take(ScriptPromiseResolver*,
                             const NFCErrorType& errorType) {
  switch (errorType) {
    case NFCErrorType::SECURITY:
      return DOMException::create(SecurityError, "NFC operation not allowed.");
    case NFCErrorType::NOT_SUPPORTED:
    case NFCErrorType::DEVICE_DISABLED:
      return DOMException::create(NotSupportedError,
                                  "NFC operation not supported.");
    case NFCErrorType::NOT_FOUND:
      return DOMException::create(NotFoundError,
                                  "Invalid NFC watch Id was provided.");
    case NFCErrorType::INVALID_MESSAGE:
      return DOMException::create(SyntaxError,
                                  "Invalid NFC message was provided.");
    case NFCErrorType::OPERATION_CANCELLED:
      return DOMException::create(AbortError,
                                  "The NFC operation was cancelled.");
    case NFCErrorType::TIMER_EXPIRED:
      return DOMException::create(TimeoutError, "NFC operation has timed-out.");
    case NFCErrorType::CANNOT_CANCEL:
      return DOMException::create(NoModificationAllowedError,
                                  "NFC operation cannot be canceled.");
    case NFCErrorType::IO_ERROR:
      return DOMException::create(NetworkError,
                                  "NFC data transfer error has occurred.");
  }
  NOTREACHED();
  return DOMException::create(UnknownError,
                              "An unknown NFC error has occurred.");
}

}  // namespace blink
