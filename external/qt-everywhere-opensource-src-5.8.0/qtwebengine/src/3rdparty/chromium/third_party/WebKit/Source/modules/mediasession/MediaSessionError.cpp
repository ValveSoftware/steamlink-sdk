// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediasession/MediaSessionError.h"

#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "public/platform/modules/mediasession/WebMediaSessionError.h"

namespace blink {

// static
DOMException* MediaSessionError::take(ScriptPromiseResolver*, const WebMediaSessionError& webError)
{
    DCHECK(webError == WebMediaSessionError::Activate);
    return DOMException::create(InvalidStateError, "The media session activation failed.");
}

} // namespace blink
