// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/v8/V8DOMActivityLogger.h"

#include "bindings/v8/V8Binding.h"
#include "platform/weborigin/KURL.h"
#include "wtf/HashMap.h"
#include "wtf/MainThread.h"
#include "wtf/text/StringHash.h"

namespace WebCore {

typedef HashMap<String, OwnPtr<V8DOMActivityLogger> > DOMActivityLoggerMapForMainWorld;
typedef HashMap<int, OwnPtr<V8DOMActivityLogger>, WTF::IntHash<int>, WTF::UnsignedWithZeroKeyHashTraits<int> > DOMActivityLoggerMapForIsolatedWorld;

static DOMActivityLoggerMapForMainWorld& domActivityLoggersForMainWorld()
{
    ASSERT(isMainThread());
    DEFINE_STATIC_LOCAL(DOMActivityLoggerMapForMainWorld, map, ());
    return map;
}

static DOMActivityLoggerMapForIsolatedWorld& domActivityLoggersForIsolatedWorld()
{
    ASSERT(isMainThread());
    DEFINE_STATIC_LOCAL(DOMActivityLoggerMapForIsolatedWorld, map, ());
    return map;
}

void V8DOMActivityLogger::setActivityLogger(int worldId, const String& extensionId, PassOwnPtr<V8DOMActivityLogger> logger)
{
    if (worldId)
        domActivityLoggersForIsolatedWorld().set(worldId, logger);
    else
        domActivityLoggersForMainWorld().set(extensionId, logger);
}

V8DOMActivityLogger* V8DOMActivityLogger::activityLogger(int worldId, const String& extensionId)
{
    if (worldId) {
        DOMActivityLoggerMapForIsolatedWorld& loggers = domActivityLoggersForIsolatedWorld();
        DOMActivityLoggerMapForIsolatedWorld::iterator it = loggers.find(worldId);
        return it == loggers.end() ? 0 : it->value.get();
    }

    if (extensionId.isEmpty())
        return 0;

    DOMActivityLoggerMapForMainWorld& loggers = domActivityLoggersForMainWorld();
    DOMActivityLoggerMapForMainWorld::iterator it = loggers.find(extensionId);
    return it == loggers.end() ? 0 : it->value.get();
}

V8DOMActivityLogger* V8DOMActivityLogger::activityLogger(int worldId, const KURL& url)
{
    // extension ID is ignored for worldId != 0.
    if (worldId)
        return activityLogger(worldId, String());

    // To find an activity logger that corresponds to the main world of an
    // extension, we need to obtain the extension ID. Extension ID is a hostname
    // of a background page's URL.
    if (!url.protocolIs("chrome-extension"))
        return 0;

    return activityLogger(worldId, url.host());
}

} // namespace WebCore
