// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Carbon/Carbon.h>

#include "build/build_config.h"

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "content/browser/browser_child_process_host_impl.h"
#include "content/browser/plugin_process_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "ui/gfx/rect.h"

namespace content {

void PluginProcessHost::OnPluginSelectWindow(uint32 window_id,
                                             gfx::Rect window_rect,
                                             bool modal) {
  plugin_visible_windows_set_.insert(window_id);
  if (modal)
    plugin_modal_windows_set_.insert(window_id);
}

void PluginProcessHost::OnPluginShowWindow(uint32 window_id,
                                           gfx::Rect window_rect,
                                           bool modal) {
  plugin_visible_windows_set_.insert(window_id);
  if (modal)
    plugin_modal_windows_set_.insert(window_id);
  CGRect window_bounds = {
    { window_rect.x(), window_rect.y() },
    { window_rect.width(), window_rect.height() }
  };
  CGRect main_display_bounds = CGDisplayBounds(CGMainDisplayID());
  if (CGRectEqualToRect(window_bounds, main_display_bounds) &&
      (plugin_fullscreen_windows_set_.find(window_id) ==
       plugin_fullscreen_windows_set_.end())) {
    plugin_fullscreen_windows_set_.insert(window_id);
    // If the plugin has just shown a window that's the same dimensions as
    // the main display, hide the menubar so that it has the whole screen.
    // (but only if we haven't already seen this fullscreen window, since
    // otherwise our refcounting can get skewed).
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(base::mac::RequestFullScreen,
                                       base::mac::kFullScreenModeHideAll));
  }
}

// Must be called on the UI thread.
// If plugin_pid is -1, the browser will be the active process on return,
// otherwise that process will be given focus back before this function returns.
static void ReleasePluginFullScreen(pid_t plugin_pid) {
  // Releasing full screen only works if we are the frontmost process; grab
  // focus, but give it back to the plugin process if requested.
  base::mac::ActivateProcess(base::GetCurrentProcId());
  base::mac::ReleaseFullScreen(base::mac::kFullScreenModeHideAll);
  if (plugin_pid != -1) {
    base::mac::ActivateProcess(plugin_pid);
  }
}

void PluginProcessHost::OnPluginHideWindow(uint32 window_id,
                                           gfx::Rect window_rect) {
  bool had_windows = !plugin_visible_windows_set_.empty();
  plugin_visible_windows_set_.erase(window_id);
  bool browser_needs_activation = had_windows &&
      plugin_visible_windows_set_.empty();

  plugin_modal_windows_set_.erase(window_id);
  if (plugin_fullscreen_windows_set_.find(window_id) !=
      plugin_fullscreen_windows_set_.end()) {
    plugin_fullscreen_windows_set_.erase(window_id);
    pid_t plugin_pid =
        browser_needs_activation ? -1 : process_->GetData().handle;
    browser_needs_activation = false;
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(ReleasePluginFullScreen, plugin_pid));
  }

  if (browser_needs_activation) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(base::mac::ActivateProcess, base::GetCurrentProcId()));
  }
}

void PluginProcessHost::OnAppActivation() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // If our plugin process has any modal windows up, we need to bring it forward
  // so that they act more like an in-process modal window would.
  if (!plugin_modal_windows_set_.empty()) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(base::mac::ActivateProcess, process_->GetData().handle));
  }
}

void PluginProcessHost::OnPluginSetCursorVisibility(bool visible) {
  if (plugin_cursor_visible_ != visible) {
    plugin_cursor_visible_ = visible;
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(base::mac::SetCursorVisibility,
                                       visible));
  }
}

}  // namespace content
