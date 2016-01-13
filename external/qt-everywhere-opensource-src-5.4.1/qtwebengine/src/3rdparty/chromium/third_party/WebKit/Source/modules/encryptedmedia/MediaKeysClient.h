// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaKeysClient_h
#define MediaKeysClient_h

#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {
class WebContentDecryptionModule;
}

namespace WebCore {

class ExecutionContext;
class Page;

class MediaKeysClient {
public:
    virtual PassOwnPtr<blink::WebContentDecryptionModule> createContentDecryptionModule(ExecutionContext*, const String& keySystem) = 0;

protected:
    virtual ~MediaKeysClient() { }
};

void provideMediaKeysTo(Page&, MediaKeysClient*);

} // namespace WebCore

#endif // MediaKeysClient_h

