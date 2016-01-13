// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_state_impl.h"

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/timer/timer.h"
#include "content/browser/accessibility/accessibility_mode_helper.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/common/content_switches.h"
#include "ui/gfx/sys_color_change_listener.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace content {

// Update the accessibility histogram 45 seconds after initialization.
static const int kAccessibilityHistogramDelaySecs = 45;

// static
BrowserAccessibilityState* BrowserAccessibilityState::GetInstance() {
  return BrowserAccessibilityStateImpl::GetInstance();
}

// static
BrowserAccessibilityStateImpl* BrowserAccessibilityStateImpl::GetInstance() {
  return Singleton<BrowserAccessibilityStateImpl,
                   LeakySingletonTraits<BrowserAccessibilityStateImpl> >::get();
}

BrowserAccessibilityStateImpl::BrowserAccessibilityStateImpl()
    : BrowserAccessibilityState(),
      accessibility_mode_(AccessibilityModeOff) {
  ResetAccessibilityModeValue();
#if defined(OS_WIN)
  // On Windows, UpdateHistograms calls some system functions with unknown
  // runtime, so call it on the file thread to ensure there's no jank.
  // Everything in that method must be safe to call on another thread.
  BrowserThread::ID update_histogram_thread = BrowserThread::FILE;
#else
  // On all other platforms, UpdateHistograms should be called on the main
  // thread.
  BrowserThread::ID update_histogram_thread = BrowserThread::UI;
#endif

  // We need to AddRef() the leaky singleton so that Bind doesn't
  // delete it prematurely.
  AddRef();
  BrowserThread::PostDelayedTask(
      update_histogram_thread, FROM_HERE,
      base::Bind(&BrowserAccessibilityStateImpl::UpdateHistograms, this),
      base::TimeDelta::FromSeconds(kAccessibilityHistogramDelaySecs));
}

BrowserAccessibilityStateImpl::~BrowserAccessibilityStateImpl() {
}

void BrowserAccessibilityStateImpl::OnScreenReaderDetected() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableRendererAccessibility)) {
    return;
  }
  EnableAccessibility();
}

void BrowserAccessibilityStateImpl::EnableAccessibility() {
  AddAccessibilityMode(AccessibilityModeComplete);
}

void BrowserAccessibilityStateImpl::DisableAccessibility() {
  ResetAccessibilityMode();
}

void BrowserAccessibilityStateImpl::ResetAccessibilityModeValue() {
  accessibility_mode_ = AccessibilityModeOff;
#if defined(OS_WIN)
  // On Windows 8, always enable accessibility for editable text controls
  // so we can show the virtual keyboard when one is enabled.
  if (base::win::GetVersion() >= base::win::VERSION_WIN8 &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableRendererAccessibility)) {
    accessibility_mode_ = AccessibilityModeEditableTextOnly;
  }
#endif  // defined(OS_WIN)

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceRendererAccessibility)) {
    accessibility_mode_ = AccessibilityModeComplete;
  }
}

void BrowserAccessibilityStateImpl::ResetAccessibilityMode() {
  ResetAccessibilityModeValue();

  // Iterate over all RenderWidgetHosts, even swapped out ones in case
  // they become active again.
  scoped_ptr<RenderWidgetHostIterator> widgets(
      RenderWidgetHostImpl::GetAllRenderWidgetHosts());
  while (RenderWidgetHost* widget = widgets->GetNextHost()) {
    // Ignore processes that don't have a connection, such as crashed tabs.
    if (!widget->GetProcess()->HasConnection())
      continue;
    if (!widget->IsRenderView())
      continue;

    RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(widget);
    rwhi->ResetAccessibilityMode();
  }
}

bool BrowserAccessibilityStateImpl::IsAccessibleBrowser() {
  return ((accessibility_mode_ & AccessibilityModeComplete) ==
          AccessibilityModeComplete);
}

void BrowserAccessibilityStateImpl::AddHistogramCallback(
    base::Closure callback) {
  histogram_callbacks_.push_back(callback);
}

void BrowserAccessibilityStateImpl::UpdateHistogramsForTesting() {
  UpdateHistograms();
}

void BrowserAccessibilityStateImpl::UpdateHistograms() {
  UpdatePlatformSpecificHistograms();

  for (size_t i = 0; i < histogram_callbacks_.size(); ++i)
    histogram_callbacks_[i].Run();

  UMA_HISTOGRAM_BOOLEAN("Accessibility.State", IsAccessibleBrowser());
  UMA_HISTOGRAM_BOOLEAN("Accessibility.InvertedColors",
                        gfx::IsInvertedColorScheme());
  UMA_HISTOGRAM_BOOLEAN("Accessibility.ManuallyEnabled",
                        CommandLine::ForCurrentProcess()->HasSwitch(
                            switches::kForceRendererAccessibility));
}

#if !defined(OS_WIN)
void BrowserAccessibilityStateImpl::UpdatePlatformSpecificHistograms() {
}
#endif

void BrowserAccessibilityStateImpl::AddAccessibilityMode(
    AccessibilityMode mode) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableRendererAccessibility)) {
    return;
  }

  accessibility_mode_ =
      content::AddAccessibilityModeTo(accessibility_mode_, mode);

  AddOrRemoveFromRenderWidgets(mode, true);
}

void BrowserAccessibilityStateImpl::RemoveAccessibilityMode(
    AccessibilityMode mode) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceRendererAccessibility) &&
      mode == AccessibilityModeComplete) {
    return;
  }

  accessibility_mode_ =
      content::RemoveAccessibilityModeFrom(accessibility_mode_, mode);

  AddOrRemoveFromRenderWidgets(mode, false);
}

void BrowserAccessibilityStateImpl::AddOrRemoveFromRenderWidgets(
    AccessibilityMode mode,
    bool add) {
  // Iterate over all RenderWidgetHosts, even swapped out ones in case
  // they become active again.
  scoped_ptr<RenderWidgetHostIterator> widgets(
      RenderWidgetHostImpl::GetAllRenderWidgetHosts());
  while (RenderWidgetHost* widget = widgets->GetNextHost()) {
    // Ignore processes that don't have a connection, such as crashed tabs.
    if (!widget->GetProcess()->HasConnection())
      continue;
    if (!widget->IsRenderView())
      continue;

    RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(widget);
    if (add)
      rwhi->AddAccessibilityMode(mode);
    else
      rwhi->RemoveAccessibilityMode(mode);
  }
}

}  // namespace content
