/*
 * Copyright (C) 2015 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#ifndef RTCCertificate_h
#define RTCCertificate_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/DOMTimeStamp.h"
#include "platform/heap/GarbageCollected.h"
#include "public/platform/WebRTCCertificate.h"
#include <memory>

namespace blink {

class RTCCertificate final : public GarbageCollectedFinalized<RTCCertificate>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    // Takes ownership of the certificate.
    RTCCertificate(std::unique_ptr<WebRTCCertificate>);

    // Returns a new WebRTCCertificate shallow copy.
    std::unique_ptr<WebRTCCertificate> certificateShallowCopy() const;
    const WebRTCCertificate& certificate() const { return *m_certificate; }

    DEFINE_INLINE_TRACE() {}

    // Returns the expiration time in ms relative to epoch, 1970-01-01T00:00:00Z.
    DOMTimeStamp expires() const;

private:
    std::unique_ptr<WebRTCCertificate> m_certificate;
};

} // namespace blink

#endif // RTCCertificate_h
