// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_SHOW_DESKTOP_NOTIFICATION_PARAMS_H_
#define CONTENT_PUBLIC_COMMON_SHOW_DESKTOP_NOTIFICATION_PARAMS_H_

#include "content/common/content_export.h"
#include "third_party/WebKit/public/web/WebTextDirection.h"
#include "url/gurl.h"

namespace content {

// Parameters used when showing an HTML5 notification.
struct CONTENT_EXPORT ShowDesktopNotificationHostMsgParams {
  ShowDesktopNotificationHostMsgParams();
  ~ShowDesktopNotificationHostMsgParams();

  // URL which is the origin that created this notification.
  GURL origin;

  // Contents of the notification if is_html is false.
  GURL icon_url;
  base::string16 title;
  base::string16 body;

  // Directionality of the notification.
  blink::WebTextDirection direction;

  // ReplaceID if this notification should replace an existing one; may be
  // empty if no replacement is called for.
  base::string16 replace_id;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_SHOW_DESKTOP_NOTIFICATION_PARAMS_H_
