// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PresentationAvailabilityCallbacks_h
#define PresentationAvailabilityCallbacks_h

#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebCallbacks.h"
#include "wtf/Noncopyable.h"

namespace blink {

class ScriptPromiseResolver;
struct WebPresentationError;

// PresentationAvailabilityCallback extends WebCallbacks to resolve the
// underlying promise depending on the result passed to the callback. It takes a
// KURL in its constructor and will pass it to the WebAvailabilityObserver.
class PresentationAvailabilityCallbacks final : public WebCallbacks<bool, const WebPresentationError&> {
public:
    PresentationAvailabilityCallbacks(ScriptPromiseResolver*, const KURL&);
    ~PresentationAvailabilityCallbacks() override;

    void onSuccess(bool value) override;
    void onError(const WebPresentationError&) override;

private:
    Persistent<ScriptPromiseResolver> m_resolver;
    const KURL m_url;

    WTF_MAKE_NONCOPYABLE(PresentationAvailabilityCallbacks);
};

} // namespace blink

#endif // PresentationAvailabilityCallbacks_h
