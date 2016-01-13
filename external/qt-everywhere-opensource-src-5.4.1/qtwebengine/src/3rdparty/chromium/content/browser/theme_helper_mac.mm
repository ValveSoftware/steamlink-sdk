// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/theme_helper_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/command_line.h"
#include "base/mac/sdk_forward_declarations.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_switches.h"

// Declare notification names from the 10.7 SDK.
#if !defined(MAC_OS_X_VERSION_10_7) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7

NSString* NSPreferredScrollerStyleDidChangeNotification =
    @"NSPreferredScrollerStyleDidChangeNotification";

#endif

@interface ScrollbarPrefsObserver : NSObject

+ (void)registerAsObserver;
+ (void)appearancePrefsChanged:(NSNotification*)notification;
+ (void)behaviorPrefsChanged:(NSNotification*)notification;
+ (void)notifyPrefsChangedWithRedraw:(BOOL)redraw;

@end

@implementation ScrollbarPrefsObserver

+ (void)registerAsObserver {
  [[NSDistributedNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(appearancePrefsChanged:)
             name:@"AppleAquaScrollBarVariantChanged"
           object:nil
suspensionBehavior:NSNotificationSuspensionBehaviorDeliverImmediately];

  [[NSDistributedNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(behaviorPrefsChanged:)
             name:@"AppleNoRedisplayAppearancePreferenceChanged"
           object:nil
suspensionBehavior:NSNotificationSuspensionBehaviorCoalesce];

  // In single-process mode, renderers will catch these notifications
  // themselves and listening for them here may trigger the DCHECK in Observe().
  if ([NSScroller respondsToSelector:@selector(preferredScrollerStyle)] &&
      !CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess)) {
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(behaviorPrefsChanged:)
               name:NSPreferredScrollerStyleDidChangeNotification
             object:nil];
  }
}

+ (void)appearancePrefsChanged:(NSNotification*)notification {
  [self notifyPrefsChangedWithRedraw:YES];
}

+ (void)behaviorPrefsChanged:(NSNotification*)notification {
  [self notifyPrefsChangedWithRedraw:NO];
}

+ (void)notifyPrefsChangedWithRedraw:(BOOL)redraw {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults synchronize];

  content::ThemeHelperMac::SendThemeChangeToAllRenderers(
      [defaults floatForKey:@"NSScrollerButtonDelay"],
      [defaults floatForKey:@"NSScrollerButtonPeriod"],
      [defaults boolForKey:@"AppleScrollerPagingBehavior"],
      content::ThemeHelperMac::GetPreferredScrollerStyle(),
      redraw);
}

@end

namespace content {

// static
ThemeHelperMac* ThemeHelperMac::GetInstance() {
  return Singleton<ThemeHelperMac,
      LeakySingletonTraits<ThemeHelperMac> >::get();
}

// static
blink::ScrollerStyle ThemeHelperMac::GetPreferredScrollerStyle() {
  if (![NSScroller respondsToSelector:@selector(preferredScrollerStyle)])
    return blink::ScrollerStyleLegacy;
  return static_cast<blink::ScrollerStyle>([NSScroller preferredScrollerStyle]);
}

// static
void ThemeHelperMac::SendThemeChangeToAllRenderers(
    float initial_button_delay,
    float autoscroll_button_delay,
    bool jump_on_track_click,
    blink::ScrollerStyle preferred_scroller_style,
    bool redraw) {
  for (RenderProcessHost::iterator it(RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd();
       it.Advance()) {
    it.GetCurrentValue()->Send(new ViewMsg_UpdateScrollbarTheme(
        initial_button_delay,
        autoscroll_button_delay,
        jump_on_track_click,
        preferred_scroller_style,
        redraw));
  }
}

ThemeHelperMac::ThemeHelperMac() {
  [ScrollbarPrefsObserver registerAsObserver];
  registrar_.Add(this,
                 NOTIFICATION_RENDERER_PROCESS_CREATED,
                 NotificationService::AllSources());
}

ThemeHelperMac::~ThemeHelperMac() {
}

void ThemeHelperMac::Observe(int type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  DCHECK_EQ(NOTIFICATION_RENDERER_PROCESS_CREATED, type);

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults synchronize];

  RenderProcessHost* rph = Source<RenderProcessHost>(source).ptr();
  rph->Send(new ViewMsg_UpdateScrollbarTheme(
      [defaults floatForKey:@"NSScrollerButtonDelay"],
      [defaults floatForKey:@"NSScrollerButtonPeriod"],
      [defaults boolForKey:@"AppleScrollerPagingBehavior"],
      GetPreferredScrollerStyle(),
      false));
}

}  // namespace content
