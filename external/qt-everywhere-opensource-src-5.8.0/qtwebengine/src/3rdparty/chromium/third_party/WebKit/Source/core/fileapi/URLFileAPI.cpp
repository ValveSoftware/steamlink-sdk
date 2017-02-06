// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fileapi/URLFileAPI.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/DOMURL.h"
#include "core/dom/ExecutionContext.h"
#include "core/fileapi/Blob.h"
#include "core/html/PublicURLManager.h"

namespace blink {

// static
String URLFileAPI::createObjectURL(ExecutionContext* executionContext, Blob* blob, ExceptionState& exceptionState)
{
    DCHECK(blob);
    DCHECK(executionContext);

    if (blob->isClosed()) {
        exceptionState.throwDOMException(InvalidStateError, String(blob->isFile() ? "File" : "Blob") + " has been closed.");
        return String();
    }
    return DOMURL::createPublicURL(executionContext, blob, blob->uuid());
}

// static
void URLFileAPI::revokeObjectURL(ExecutionContext* executionContext, const String& urlString)
{
    DCHECK(executionContext);

    KURL url(KURL(), urlString);
    executionContext->removeURLFromMemoryCache(url);
    executionContext->publicURLManager().revoke(url);
}

} // namespace blink
