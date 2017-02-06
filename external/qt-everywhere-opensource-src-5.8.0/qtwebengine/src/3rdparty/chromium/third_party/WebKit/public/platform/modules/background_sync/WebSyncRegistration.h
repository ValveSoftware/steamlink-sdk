// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebSyncRegistration_h
#define WebSyncRegistration_h

#include "public/platform/WebPrivatePtr.h"
#include "public/platform/WebString.h"

namespace blink {

struct WebSyncRegistration {
    enum NetworkState {
        NetworkStateAny = 0,
        NetworkStateAvoidCellular,
        NetworkStateOnline,
        NetworkStateLast = NetworkStateOnline
    };

    enum { UNREGISTERED_SYNC_ID = -1};

    WebSyncRegistration()
        : id(UNREGISTERED_SYNC_ID)
        , tag("")
        , networkState(NetworkState::NetworkStateOnline)
    {
    }

    WebSyncRegistration(int64_t id,
        const WebString& registrationTag,
        NetworkState networkState)
        : id(id)
        , tag(registrationTag)
        , networkState(networkState)
    {
    }

    /* Internal identity; not exposed to JS API. */
    int64_t id;

    WebString tag;
    NetworkState networkState;
};

} // namespace blink

#endif // WebSyncRegistration_h
