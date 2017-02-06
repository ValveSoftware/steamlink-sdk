// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_state_impl.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "build/build_config.h"
#include "content/browser/accessibility/accessibility_mode_helper.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "ui/gfx/color_utils.h"

namespace content {

// Update the accessibility histogram 45 seconds after initialization.
static const int kAccessibilityHistogramDelaySecs = 45;

// static
BrowserAccessibilityState* BrowserAccessibilityState::GetInstance() {
  return BrowserAccessibilityStateImpl::GetInstance();
}

// static
BrowserAccessibilityStateImpl* BrowserAccessibilityStateImpl::GetInstance() {
  return base::Singleton<
      BrowserAccessibilityStateImpl,
      base::LeakySingletonTraits<BrowserAccessibilityStateImpl>>::get();
}

BrowserAccessibilityStateImpl::BrowserAccessibilityStateImpl()
    : BrowserAccessibilityState(),
      accessibility_mode_(AccessibilityModeOff),
      disable_hot_tracking_(false) {
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
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
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
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceRendererAccessibility)) {
    accessibility_mode_ = AccessibilityModeComplete;
  }
}

void BrowserAccessibilityStateImpl::ResetAccessibilityMode() {
  ResetAccessibilityModeValue();

  std::vector<WebContentsImpl*> web_contents_vector =
      WebContentsImpl::GetAllWebContents();
  for (size_t i = 0; i < web_contents_vector.size(); ++i)
    web_contents_vector[i]->SetAccessibilityMode(accessibility_mode());
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
                        color_utils::IsInvertedColorScheme());
  UMA_HISTOGRAM_BOOLEAN("Accessibility.ManuallyEnabled",
                        base::CommandLine::ForCurrentProcess()->HasSwitch(
                            switches::kForceRendererAccessibility));
}

#if !defined(OS_WIN) && !defined(OS_MACOSX)
void BrowserAccessibilityStateImpl::UpdatePlatformSpecificHistograms() {
}
#endif

void BrowserAccessibilityStateImpl::AddAccessibilityMode(
    AccessibilityMode mode) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableRendererAccessibility)) {
    return;
  }

  accessibility_mode_ =
      content::AddAccessibilityModeTo(accessibility_mode_, mode);

  AddOrRemoveFromAllWebContents(mode, true);
}

void BrowserAccessibilityStateImpl::RemoveAccessibilityMode(
    AccessibilityMode mode) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceRendererAccessibility) &&
      mode == AccessibilityModeComplete) {
    return;
  }

  accessibility_mode_ =
      content::RemoveAccessibilityModeFrom(accessibility_mode_, mode);

  AddOrRemoveFromAllWebContents(mode, false);
}

void BrowserAccessibilityStateImpl::AddOrRemoveFromAllWebContents(
    AccessibilityMode mode,
    bool add) {
  std::vector<WebContentsImpl*> web_contents_vector =
      WebContentsImpl::GetAllWebContents();
  for (size_t i = 0; i < web_contents_vector.size(); ++i) {
    if (add)
      web_contents_vector[i]->AddAccessibilityMode(mode);
    else
      web_contents_vector[i]->RemoveAccessibilityMode(mode);
  }
}

}  // namespace content
