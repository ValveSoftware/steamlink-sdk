// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorNetworkInformation_h
#define NavigatorNetworkInformation_h

#include "core/frame/DOMWindowProperty.h"
#include "platform/Supplementable.h"

namespace WebCore {

class Navigator;
class NetworkInformation;

class NavigatorNetworkInformation FINAL
    : public NoBaseWillBeGarbageCollectedFinalized<NavigatorNetworkInformation>
    , public WillBeHeapSupplement<Navigator>
    , DOMWindowProperty {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(NavigatorNetworkInformation);

public:
    virtual ~NavigatorNetworkInformation();
    static NavigatorNetworkInformation& from(Navigator&);
    static NavigatorNetworkInformation* toNavigatorNetworkInformation(Navigator&);
    static const char* supplementName();

    static NetworkInformation* connection(Navigator&);

    virtual void trace(Visitor*) OVERRIDE;

private:
    explicit NavigatorNetworkInformation(Navigator&);
    NetworkInformation* connection();

    RefPtrWillBeMember<NetworkInformation> m_connection;
};

} // namespace WebCore

#endif // NavigatorNetworkInformation_h
