// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PresentationConnectionCallbacks_h
#define PresentationConnectionCallbacks_h

#include "platform/heap/Handle.h"
#include "public/platform/WebCallbacks.h"
#include "wtf/Noncopyable.h"

namespace blink {

class PresentationRequest;
class ScriptPromiseResolver;
class WebPresentationConnectionClient;
struct WebPresentationError;

// PresentationConnectionCallbacks extends WebCallbacks to resolve the
// underlying promise depending on the result passed to the callback. It takes
// the PresentationRequest object that originated the call in its constructor
// and will pass it to the created PresentationConnection.
class PresentationConnectionCallbacks final
    : public WebCallbacks<std::unique_ptr<WebPresentationConnectionClient>, const WebPresentationError&> {
public:
    PresentationConnectionCallbacks(ScriptPromiseResolver*, PresentationRequest*);
    ~PresentationConnectionCallbacks() override = default;

    void onSuccess(std::unique_ptr<WebPresentationConnectionClient>) override;
    void onError(const WebPresentationError&) override;

private:
    Persistent<ScriptPromiseResolver> m_resolver;
    Persistent<PresentationRequest> m_request;

    WTF_MAKE_NONCOPYABLE(PresentationConnectionCallbacks);
};

} // namespace blink

#endif // PresentationConnectionCallbacks_h
