// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SecurityOriginStructTraits_h
#define SecurityOriginStructTraits_h

#include "platform/weborigin/SecurityOrigin.h"
#include "url/mojo/origin.mojom-blink.h"
#include "wtf/text/WTFString.h"

namespace mojo {

template <>
struct StructTraits<url::mojom::blink::Origin, RefPtr<::blink::SecurityOrigin>> {
    static WTF::String scheme(const RefPtr<::blink::SecurityOrigin>& origin)
    {
        return origin->protocol();
    }
    static WTF::String host(const RefPtr<::blink::SecurityOrigin>& origin)
    {
        return origin->host();
    }
    static uint16_t port(const RefPtr<::blink::SecurityOrigin>& origin)
    {
        return origin->port();
    }
    static bool unique(const RefPtr<::blink::SecurityOrigin>& origin)
    {
        return origin->isUnique();
    }
    static bool Read(url::mojom::blink::OriginDataView data, RefPtr<::blink::SecurityOrigin>* out)
    {
        if (data.unique()) {
            *out = ::blink::SecurityOrigin::createUnique();
        } else {
            WTF::String scheme;
            WTF::String host;
            if (!data.ReadScheme(&scheme) || !data.ReadHost(&host))
                return false;

            *out = ::blink::SecurityOrigin::create(scheme, host, data.port());
        }

        // If a unique origin was created, but the unique flag wasn't set, then
        // the values provided to 'create' were invalid.
        if (!data.unique() && (*out)->isUnique())
            return false;

        return true;
    }
};
}

#endif // SecurityOriginStructTraits_h
