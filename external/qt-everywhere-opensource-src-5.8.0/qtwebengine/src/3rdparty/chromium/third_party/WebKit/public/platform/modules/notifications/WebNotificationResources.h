// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebNotificationResources_h
#define WebNotificationResources_h

#include "public/platform/WebVector.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace blink {

// Structure representing the resources associated with a Web Notification.
struct WebNotificationResources {
    // Main icon for the notification. The bitmap may be empty if the developer
    // did not provide an icon, or fetching of the icon failed.
    SkBitmap icon;

    // Badge for the notification. The bitmap may be empty if the developer
    // did not provide a badge, or fetching of the badge failed.
    SkBitmap badge;

    // Icons for the actions. A bitmap may be empty if the developer did not
    // provide an icon, or fetching of the icon failed.
    WebVector<SkBitmap> actionIcons;
};

} // namespace blink

#endif // WebNotificationResources_h
