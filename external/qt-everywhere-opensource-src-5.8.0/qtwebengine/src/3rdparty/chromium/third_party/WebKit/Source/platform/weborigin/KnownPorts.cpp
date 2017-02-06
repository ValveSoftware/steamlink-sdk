/*
 * Copyright (C) 2004, 2007, 2008, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/weborigin/KnownPorts.h"

#include "net/base/port_util.h"
#include "platform/weborigin/KURL.h"
#include "wtf/text/StringUTF8Adaptor.h"
#include "wtf/text/WTFString.h"

namespace blink {

bool isDefaultPortForProtocol(unsigned short port, const WTF::String& protocol)
{
    if (protocol.isEmpty())
        return false;

    switch (port) {
    case 80:
        return protocol == "http" || protocol == "ws";
    case 443:
        return protocol == "https" || protocol == "wss";
    case 21:
        return protocol == "ftp";
    case 990:
        return protocol == "ftps";
    }
    return false;
}

unsigned short defaultPortForProtocol(const WTF::String& protocol)
{
    if (protocol == "http" || protocol == "ws")
        return 80;
    if (protocol == "https" || protocol == "wss")
        return 443;
    if (protocol == "ftp")
        return 21;
    if (protocol == "ftps")
        return 990;

    return 0;
}

bool isPortAllowedForScheme(const KURL& url)
{
    // Returns true for URLs without a port specified. This is needed to let
    // through non-network schemes that don't go over the network.
    if (!url.hasPort())
        return true;
    String protocol = url.protocol().isNull() ? "" : url.protocol().lower();
    unsigned short effectivePort = url.port();
    if (!effectivePort)
        effectivePort = defaultPortForProtocol(protocol);
    StringUTF8Adaptor utf8(protocol);
    return net::IsPortAllowedForScheme(effectivePort, std::string(utf8.data(), utf8.length()));
}

} // namespace blink
