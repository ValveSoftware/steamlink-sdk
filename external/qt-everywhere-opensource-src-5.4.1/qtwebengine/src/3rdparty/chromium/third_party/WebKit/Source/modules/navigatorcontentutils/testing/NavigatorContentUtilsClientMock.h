// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorContentUtilsClientMock_h
#define NavigatorContentUtilsClientMock_h

#include "modules/navigatorcontentutils/NavigatorContentUtilsClient.h"
#include "platform/weborigin/KURL.h"
#include "wtf/HashSet.h"
#include "wtf/OwnPtr.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

// Provides a mock object for the navigatorcontentutils client.
class NavigatorContentUtilsClientMock : public NavigatorContentUtilsClient {
public:
    NavigatorContentUtilsClientMock() { }
    virtual ~NavigatorContentUtilsClientMock() { }

    virtual void registerProtocolHandler(const String& scheme, const KURL& baseURL, const KURL&, const String& title);

    virtual CustomHandlersState isProtocolHandlerRegistered(const String& scheme, const KURL& baseURL, const KURL&);
    virtual void unregisterProtocolHandler(const String& scheme, const KURL& baseURL, const KURL&);

private:
    typedef struct {
        String scheme;
        WebCore::KURL baseURL;
        WebCore::KURL url;
        String title;
    } ProtocolInfo;

    typedef HashMap<String, ProtocolInfo> RegisteredProtocolMap;
    RegisteredProtocolMap m_protocolMap;
};

}

#endif // NavigatorContentUtilsClientMock_h
