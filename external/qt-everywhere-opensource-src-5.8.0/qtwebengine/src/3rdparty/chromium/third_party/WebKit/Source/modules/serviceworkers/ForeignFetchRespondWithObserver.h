// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ForeignFetchRespondWithObserver_h
#define ForeignFetchRespondWithObserver_h

#include "modules/serviceworkers/RespondWithObserver.h"

namespace blink {

// This class observes the service worker's handling of a ForeignFetchEvent and
// notifies the client.
class MODULES_EXPORT ForeignFetchRespondWithObserver final : public RespondWithObserver {
public:
    static ForeignFetchRespondWithObserver* create(ExecutionContext*, int eventID, const KURL& requestURL, WebURLRequest::FetchRequestMode, WebURLRequest::FrameType, WebURLRequest::RequestContext, PassRefPtr<SecurityOrigin>, WaitUntilObserver*);

    void responseWasFulfilled(const ScriptValue&) override;

private:
    ForeignFetchRespondWithObserver(ExecutionContext*, int eventID, const KURL& requestURL, WebURLRequest::FetchRequestMode, WebURLRequest::FrameType, WebURLRequest::RequestContext, PassRefPtr<SecurityOrigin>, WaitUntilObserver*);

    RefPtr<SecurityOrigin> m_requestOrigin;
};

} // namespace blink

#endif // ForeignFetchRespondWithObserver_h
