// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STARTUP_METRIC_UTILS_BROWSER_STARTUP_METRIC_UTILS_H_
#define COMPONENTS_STARTUP_METRIC_UTILS_BROWSER_STARTUP_METRIC_UTILS_H_

#include <stdint.h>
#include <string>

#include "base/time/time.h"
#include "build/build_config.h"

class PrefRegistrySimple;
class PrefService;

// Utility functions to support metric collection for browser startup. Timings
// should use TimeTicks whenever possible. OS-provided timings are still
// received as Time out of cross-platform support necessity but are converted to
// TimeTicks as soon as possible in an attempt to reduce the potential skew
// between the two basis. See crbug.com/544131 for reasoning.

namespace startup_metric_utils {

// An enumeration of startup temperatures. This must be kept in sync with the
// UMA StartupType enumeration defined in histograms.xml.
enum StartupTemperature {
  // The startup was a cold start: nearly all of the binaries and resources were
  // brought into memory using hard faults.
  COLD_STARTUP_TEMPERATURE = 0,
  // The startup was a warm start: the binaries and resources were mostly
  // already resident in memory and effectively no hard faults were observed.
  WARM_STARTUP_TEMPERATURE = 1,
  // The startup type couldn't quite be classified as warm or cold, but rather
  // was somewhere in between.
  LUKEWARM_STARTUP_TEMPERATURE = 2,
  // This must be after all meaningful values. All new values should be added
  // above this one.
  STARTUP_TEMPERATURE_COUNT,
  // Startup temperature wasn't yet determined.
  UNDETERMINED_STARTUP_TEMPERATURE
};

#if defined(OS_WIN)
// Gets the hard fault count of the current process through |hard_fault_count|.
// Returns true on success.
bool GetHardFaultCountForCurrentProcess(uint32_t* hard_fault_count);
#endif  // defined(OS_WIN)

// Registers startup related prefs in |registry|.
void RegisterPrefs(PrefRegistrySimple* registry);

// Returns true if any UI other than the browser window has been displayed
// so far.  Useful to test if UI has been displayed before the first browser
// window was shown, which would invalidate any surrounding timing metrics.
bool WasNonBrowserUIDisplayed();

// Call this when displaying UI that might potentially delay startup events.
//
// Note on usage: This function is idempotent and its overhead is low enough
// in comparison with UI display that it's OK to call it on every
// UI invocation regardless of whether the browser window has already
// been displayed or not.
void SetNonBrowserUIDisplayed();

// Call this with the creation time of the startup (initial/main) process.
void RecordStartupProcessCreationTime(const base::Time& time);

// Call this with a time recorded as early as possible in the startup process.
// On Android, the entry point time is the time at which the Java code starts.
// In Mojo, the entry point time is the time at which the shell starts.
void RecordMainEntryPointTime(const base::Time& time);

// Call this with the time when the executable is loaded and main() is entered.
// Can be different from |RecordMainEntryPointTime| when the startup process is
// contained in a separate dll, such as with chrome.exe / chrome.dll on Windows.
void RecordExeMainEntryPointTime(const base::Time& time);

// Call this with the time recorded just before the message loop is started.
// |is_first_run| - is the current launch part of a first run. |pref_service| is
// an optional parameter which, if provided, will be used to store state for
// stats that span multiple startups; in its absence those stats will not be
// recorded.
void RecordBrowserMainMessageLoopStart(const base::TimeTicks& ticks,
                                       bool is_first_run,
                                       PrefService* pref_service);

// Call this with the time when the first browser window became visible.
void RecordBrowserWindowDisplay(const base::TimeTicks& ticks);

// Call this with the time delta that the browser spent opening its tabs.
void RecordBrowserOpenTabsDelta(const base::TimeDelta& delta);

// Call this with a renderer main entry time. The value provided for the first
// call to this function is used to compute
// Startup.LoadTime.BrowserMainToRendererMain. Further calls to this
// function are ignored.
void RecordRendererMainEntryTime(const base::TimeTicks& ticks);

// Call this with the time when the first web contents loaded its main frame,
// only if the first web contents was unimpended in its attempt to do so.
void RecordFirstWebContentsMainFrameLoad(const base::TimeTicks& ticks);

// Call this with the time when the first web contents had a non-empty paint,
// only if the first web contents was unimpended in its attempt to do so.
void RecordFirstWebContentsNonEmptyPaint(const base::TimeTicks& ticks);

// Call this with the time when the first web contents began navigating its main
// frame.
void RecordFirstWebContentsMainNavigationStart(const base::TimeTicks& ticks);

// Call this with the time when the first web contents successfully committed
// its navigation for the main frame.
void RecordFirstWebContentsMainNavigationFinished(const base::TimeTicks& ticks);

// Returns the TimeTicks corresponding to main entry as recorded by
// RecordMainEntryPointTime. Returns a null TimeTicks if a value has not been
// recorded yet. This method is expected to be called from the UI thread.
base::TimeTicks MainEntryPointTicks();

// Returns the startup type. This is only currently supported on the Windows
// platform and will simply return UNCERTAIN_STARTUP_TYPE on other platforms.
// This is only valid after a call to RecordBrowserMainMessageLoopStart().
StartupTemperature GetStartupTemperature();

}  // namespace startup_metric_utils

#endif  // COMPONENTS_STARTUP_METRIC_UTILS_BROWSER_STARTUP_METRIC_UTILS_H_
