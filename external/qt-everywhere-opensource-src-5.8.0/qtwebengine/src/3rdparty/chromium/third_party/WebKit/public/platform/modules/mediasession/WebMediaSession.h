// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebMediaSession_h
#define WebMediaSession_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/modules/mediasession/WebMediaSessionError.h"

namespace blink {

using WebMediaSessionActivateCallback = WebCallbacks<void, const WebMediaSessionError&>;
using WebMediaSessionDeactivateCallback = WebCallbacks<void, void>;

struct WebMediaMetadata;

class WebMediaSession {
public:
    enum {
        // The media session for media elements that don't have an
        // explicit user created media session set.
        DefaultID = 0
    };

    virtual ~WebMediaSession() = default;

    // Tries to activate the session by requesting audio focus from
    // the system. May fail if audio focus is denied by the
    // system. The ownership of the pointer is transferred to the
    // WebMediaSession implementation.
    virtual void activate(WebMediaSessionActivateCallback*) = 0;

    // Deactivates the session by abandoning audio focus. Will not
    // fail in way visible to the user of the WebMediaSession. The
    // ownership of the pointer is transferred to the WebMediaSession
    // implementation.
    virtual void deactivate(WebMediaSessionDeactivateCallback*) = 0;

    // Updates the metadata associated with the WebMediaSession. The metadata
    // can be a nullptr in which case the assouciated metadata should be reset.
    // The pointer is not owned by the WebMediaSession implementation.
    virtual void setMetadata(const WebMediaMetadata*) = 0;
};

} // namespace blink

#endif // WebMediaSession_h
