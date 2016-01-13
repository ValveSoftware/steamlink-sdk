// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ContentDecryptionModuleResult_h
#define ContentDecryptionModuleResult_h

#include "platform/heap/Handle.h"
#include "public/platform/WebContentDecryptionModuleException.h"
#include "public/platform/WebContentDecryptionModuleResult.h"

namespace blink {
class WebString;
}

namespace WebCore {

// Used to notify completion of a CDM operation.
class ContentDecryptionModuleResult : public GarbageCollectedFinalized<ContentDecryptionModuleResult> {
public:
    virtual ~ContentDecryptionModuleResult() { }

    virtual void complete() = 0;
    virtual void completeWithSession(blink::WebContentDecryptionModuleResult::SessionStatus) = 0;
    virtual void completeWithError(blink::WebContentDecryptionModuleException, unsigned long systemCode, const blink::WebString&) = 0;

    blink::WebContentDecryptionModuleResult result()
    {
        return blink::WebContentDecryptionModuleResult(this);
    }

    virtual void trace(Visitor*) { }
};

} // namespace WebCore

#endif // ContentDecryptionModuleResult_h
