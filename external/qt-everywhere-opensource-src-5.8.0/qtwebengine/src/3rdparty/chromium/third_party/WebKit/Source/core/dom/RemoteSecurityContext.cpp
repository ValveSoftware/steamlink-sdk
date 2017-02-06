// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/RemoteSecurityContext.h"

#include "core/frame/csp/ContentSecurityPolicy.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/Assertions.h"

namespace blink {

RemoteSecurityContext::RemoteSecurityContext()
    : SecurityContext()
{
    // RemoteSecurityContext's origin is expected to stay uninitialized until
    // we set it using replicated origin data from the browser process.
    DCHECK(!getSecurityOrigin());

    // Start with a clean slate.
    setContentSecurityPolicy(ContentSecurityPolicy::create());

    // FIXME: Document::initSecurityContext has a few other things we may
    // eventually want here, such as enforcing a setting to
    // grantUniversalAccess().
}

RemoteSecurityContext* RemoteSecurityContext::create()
{
    return new RemoteSecurityContext();
}

DEFINE_TRACE(RemoteSecurityContext)
{
    SecurityContext::trace(visitor);
}

void RemoteSecurityContext::setReplicatedOrigin(PassRefPtr<SecurityOrigin> origin)
{
    DCHECK(origin);
    setSecurityOrigin(origin);
    contentSecurityPolicy()->setupSelf(*getSecurityOrigin());
}

void RemoteSecurityContext::resetReplicatedContentSecurityPolicy()
{
    DCHECK(getSecurityOrigin());
    setContentSecurityPolicy(ContentSecurityPolicy::create());
    contentSecurityPolicy()->setupSelf(*getSecurityOrigin());
}

} // namespace blink
