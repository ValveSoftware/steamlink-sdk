/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WebNotification_h
#define WebNotification_h

#include "../platform/WebCommon.h"
#include "../platform/WebPrivatePtr.h"
#include "../platform/WebString.h"
#include "WebTextDirection.h"

namespace WebCore { class Notification; }

#if BLINK_IMPLEMENTATION
namespace WTF { template <typename T> class PassRefPtr; }
#endif

namespace blink {

class WebURL;

// Represents access to a desktop notification.
class WebNotification {
public:
    WebNotification() { }
    WebNotification(const WebNotification& other) { assign(other); }
    WebNotification& operator=(const WebNotification& other)
    {
        assign(other);
        return *this;
    }

    ~WebNotification() { reset(); }

    BLINK_EXPORT void reset();
    BLINK_EXPORT void assign(const WebNotification&);

    // Operators required to put WebNotification in an std::map. Mind that the
    // order of the notifications in an ordered map will be arbitrary.
    BLINK_EXPORT bool equals(const WebNotification& other) const;
    BLINK_EXPORT bool lessThan(const WebNotification& other) const;

    BLINK_EXPORT WebString title() const;

    BLINK_EXPORT WebTextDirection direction() const;
    BLINK_EXPORT WebString lang() const;
    BLINK_EXPORT WebString body() const;
    BLINK_EXPORT WebString tag() const;
    BLINK_EXPORT WebURL iconURL() const;

    // Called to indicate the notification has been displayed.
    BLINK_EXPORT void dispatchShowEvent();

    // Called to indicate an error has occurred with this notification.
    BLINK_EXPORT void dispatchErrorEvent(const WebString& errorMessage);

    // Called to indicate the notification has been closed. |byUser| indicates
    // whether it was closed by the user instead of automatically by the system.
    BLINK_EXPORT void dispatchCloseEvent(bool byUser);

    // Called to indicate the notification was clicked on.
    BLINK_EXPORT void dispatchClickEvent();

    // FIXME: Remove these methods once Chromium switched to the new APIs.
    WebString replaceId() const { return tag(); }
    void dispatchDisplayEvent() { dispatchShowEvent(); }

#if BLINK_IMPLEMENTATION
    WebNotification(WebCore::Notification*);
    WebNotification& operator=(WebCore::Notification*);
#endif

private:
    WebPrivatePtr<WebCore::Notification> m_private;
};

inline bool operator==(const WebNotification& a, const WebNotification& b)
{
    return a.equals(b);
}

inline bool operator!=(const WebNotification& a, const WebNotification& b)
{
    return !a.equals(b);
}

inline bool operator<(const WebNotification& a, const WebNotification& b)
{
    return a.lessThan(b);
}

} // namespace blink

#endif
