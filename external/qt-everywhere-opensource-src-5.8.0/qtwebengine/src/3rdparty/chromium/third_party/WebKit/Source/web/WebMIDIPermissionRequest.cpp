/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "public/web/modules/webmidi/WebMIDIPermissionRequest.h"

#include "modules/webmidi/MIDIAccessInitializer.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebSecurityOrigin.h"

namespace blink {

WebMIDIPermissionRequest::WebMIDIPermissionRequest(MIDIAccessInitializer* initializer)
    : m_private(initializer)
{
}

void WebMIDIPermissionRequest::reset()
{
    m_private.reset();
}

void WebMIDIPermissionRequest::assign(const WebMIDIPermissionRequest& other)
{
    m_private = other.m_private;
}

bool WebMIDIPermissionRequest::equals(const WebMIDIPermissionRequest& n) const
{
    return m_private.get() == n.m_private.get();
}

WebSecurityOrigin WebMIDIPermissionRequest::getSecurityOrigin() const
{
    return WebSecurityOrigin(m_private->getSecurityOrigin());
}

void WebMIDIPermissionRequest::setIsAllowed(bool allowed)
{
    m_private->resolvePermission(allowed);
}

} // namespace blink
