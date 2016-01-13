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

#ifndef WebContentDecryptionModuleSession_h
#define WebContentDecryptionModuleSession_h

#include "WebCommon.h"
#include "public/platform/WebContentDecryptionModuleException.h"
#include "public/platform/WebContentDecryptionModuleResult.h"

namespace blink {

class WebString;
class WebURL;

class BLINK_PLATFORM_EXPORT WebContentDecryptionModuleSession {
public:
    class BLINK_PLATFORM_EXPORT Client {
    public:
        enum MediaKeyErrorCode {
            MediaKeyErrorCodeUnknown = 1,
            MediaKeyErrorCodeClient,
        };

        virtual void message(const unsigned char* message, size_t messageLength, const WebURL& destinationURL) = 0;
        virtual void ready() = 0;
        virtual void close() = 0;
        // FIXME: Remove this method once Chromium updated to use the new parameters.
        virtual void error(MediaKeyErrorCode, unsigned long systemCode) = 0;
        virtual void error(WebContentDecryptionModuleException, unsigned long systemCode, const WebString& message) = 0;

    protected:
        virtual ~Client();
    };

    virtual ~WebContentDecryptionModuleSession();

    virtual WebString sessionId() const = 0;

    // FIXME: Remove these methods once the new methods are implemented in Chromium.
    virtual void initializeNewSession(const WebString& mimeType, const unsigned char* initData, size_t initDataLength) = 0;
    virtual void update(const unsigned char* response, size_t responseLength) = 0;
    virtual void release() = 0;

    virtual void initializeNewSession(const WebString& initDataType, const unsigned char* initData, size_t initDataLength, const WebString& sessionType, const WebContentDecryptionModuleResult&);
    virtual void update(const unsigned char* response, size_t responseLength, const WebContentDecryptionModuleResult&);
    virtual void release(const WebContentDecryptionModuleResult&);
};

} // namespace blink

#endif // WebContentDecryptionModuleSession_h
