// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_THEME_HELPER_MAC_H_
#define CONTENT_BROWSER_THEME_HELPER_MAC_H_

#include "base/memory/singleton.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "third_party/WebKit/public/web/mac/WebScrollbarTheme.h"

namespace content {

class ThemeHelperMac : public NotificationObserver {
 public:
  // Return pointer to the singleton instance for the current process, or NULL
  // if none.
  static ThemeHelperMac* GetInstance();

  // Returns the value of +[NSScroller preferredScrollStyle] as expressed
  // as the blink enum value.
  static blink::ScrollerStyle GetPreferredScrollerStyle();

  static void SendThemeChangeToAllRenderers(
      float initial_button_delay,
      float autoscroll_button_delay,
      bool jump_on_track_click,
      blink::ScrollerStyle preferred_scroller_style,
      bool redraw);

 private:
  friend struct DefaultSingletonTraits<ThemeHelperMac>;

  ThemeHelperMac();
  virtual ~ThemeHelperMac();

  // Overridden from NotificationObserver:
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ThemeHelperMac);
};

}  // namespace content

#endif  // CONTENT_BROWSER_THEME_HELPER_MAC_H_
