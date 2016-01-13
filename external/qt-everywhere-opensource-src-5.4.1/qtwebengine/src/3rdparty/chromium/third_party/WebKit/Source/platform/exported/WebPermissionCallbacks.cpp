// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "public/platform/WebPermissionCallbacks.h"

#include "platform/PermissionCallbacks.h"

using namespace WebCore;

namespace blink {

class WebPermissionCallbacksPrivate : public RefCounted<WebPermissionCallbacksPrivate> {
public:
    static PassRefPtr<WebPermissionCallbacksPrivate> create(const PassOwnPtr<PermissionCallbacks>& callbacks)
    {
        return adoptRef(new WebPermissionCallbacksPrivate(callbacks));
    }

    PermissionCallbacks* callbacks() { return m_callbacks.get(); }

private:
    WebPermissionCallbacksPrivate(const PassOwnPtr<PermissionCallbacks>& callbacks) : m_callbacks(callbacks) { }
    OwnPtr<PermissionCallbacks> m_callbacks;
};

WebPermissionCallbacks::WebPermissionCallbacks(const PassOwnPtr<PermissionCallbacks>& callbacks)
{
    m_private = WebPermissionCallbacksPrivate::create(callbacks);
}

void WebPermissionCallbacks::reset()
{
    m_private.reset();
}

void WebPermissionCallbacks::assign(const WebPermissionCallbacks& other)
{
    m_private = other.m_private;
}

void WebPermissionCallbacks::doAllow()
{
    ASSERT(!m_private.isNull());
    m_private->callbacks()->onAllowed();
    m_private.reset();
}

void WebPermissionCallbacks::doDeny()
{
    ASSERT(!m_private.isNull());
    m_private->callbacks()->onDenied();
    m_private.reset();
}

} // namespace blink
