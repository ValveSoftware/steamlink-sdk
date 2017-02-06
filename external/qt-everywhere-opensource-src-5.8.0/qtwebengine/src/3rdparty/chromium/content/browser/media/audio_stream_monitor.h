// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_AUDIO_STREAM_MONITOR_H_
#define CONTENT_BROWSER_MEDIA_AUDIO_STREAM_MONITOR_H_

#include <map>
#include <utility>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "media/audio/audio_output_controller.h"

namespace base {
class TickClock;
}

namespace content {

class WebContents;

// Repeatedly polls audio streams for their power levels, and "debounces" the
// information into a simple, binary "was recently audible" result for the audio
// indicators in the tab UI.  The debouncing logic is to: 1) Turn on immediately
// when sound is audible; and 2) Hold on for X amount of time after sound has
// gone silent, then turn off.  Said another way, we don't want tab indicators
// to turn on/off repeatedly and annoy the user.  AudioStreamMonitor sends UI
// update notifications only when needed, but may be queried at any time.
//
// Each WebContentsImpl owns an AudioStreamMonitor.
class CONTENT_EXPORT AudioStreamMonitor {
 public:
  explicit AudioStreamMonitor(WebContents* contents);
  ~AudioStreamMonitor();

  // Indicates if audio stream monitoring is available.  It's only available if
  // AudioOutputController can and will monitor output power levels.
  static bool monitoring_available() {
    return media::AudioOutputController::will_monitor_audio_levels();
  }

  // Returns true if audio has recently been audible from the tab.  This is
  // usually called whenever the tab data model is refreshed; but there are
  // other use cases as well (e.g., the OOM killer uses this to de-prioritize
  // the killing of tabs making sounds).
  bool WasRecentlyAudible() const;

  // Returns true if the audio is currently audible from the given WebContents.
  // The difference from WasRecentlyAudible() is that this method will return
  // false as soon as the WebContents stop producing sound.
  bool IsCurrentlyAudible() const;

  // Starts or stops audio level monitoring respectively for the stream owned by
  // the specified renderer.  Safe to call from any thread.
  //
  // The callback returns the current power level (in dBFS units) and the clip
  // status (true if any part of the audio signal has clipped since the last
  // callback run).  |stream_id| must be unique within a |render_process_id|.
  typedef base::Callback<std::pair<float, bool>()> ReadPowerAndClipCallback;
  static void StartMonitoringStream(
      int render_process_id,
      int render_frame_id,
      int stream_id,
      const ReadPowerAndClipCallback& read_power_callback);
  static void StopMonitoringStream(int render_process_id,
                                   int render_frame_id,
                                   int stream_id);

  void set_was_recently_audible_for_testing(bool value) {
    was_recently_audible_ = value;
  }

 private:
  friend class AudioStreamMonitorTest;

  enum {
    // Desired polling frequency.  Note: If this is set too low, short-duration
    // "blip" sounds won't be detected.  http://crbug.com/339133#c4
    kPowerMeasurementsPerSecond = 15,

    // Amount of time to hold a tab indicator on after its last blurt.
    kHoldOnMilliseconds = 2000
  };

  // Helper methods for starting and stopping monitoring which lookup the
  // identified renderer and forward calls to the correct AudioStreamMonitor.
  static void StartMonitoringHelper(
      int render_process_id,
      int render_frame_id,
      int stream_id,
      const ReadPowerAndClipCallback& read_power_callback);
  static void StopMonitoringHelper(int render_process_id,
                                   int render_frame_id,
                                   int stream_id);

  // Starts polling the stream for audio stream power levels using |callback|.
  void StartMonitoringStreamOnUIThread(
      int render_process_id,
      int stream_id,
      const ReadPowerAndClipCallback& callback);

  // Stops polling the stream, discarding the internal copy of the |callback|
  // provided in the call to StartMonitoringStream().
  void StopMonitoringStreamOnUIThread(int render_process_id, int stream_id);

  // Called by |poll_timer_| to sample the power levels from each of the streams
  // playing in the tab.
  void Poll();

  // Compares last known indicator state with what it should be, and triggers UI
  // updates through |web_contents_| if needed.  When the indicator is turned
  // on, |off_timer_| is started to re-invoke this method in the future.
  void MaybeToggle();

  // The WebContents instance to receive indicator toggle notifications.  This
  // pointer should be valid for the lifetime of AudioStreamMonitor.
  WebContents* const web_contents_;

  // Note: |clock_| is always |&default_tick_clock_|, except during unit
  // testing.
  base::DefaultTickClock default_tick_clock_;
  base::TickClock* const clock_;

  // Confirms single-threaded access in debug builds.
  base::ThreadChecker thread_checker_;

  // The callbacks to read power levels for each stream.  Only playing (i.e.,
  // not paused) streams will have an entry in this map.
  typedef std::pair<int, int> StreamID;
  typedef std::map<StreamID, ReadPowerAndClipCallback> StreamPollCallbackMap;
  StreamPollCallbackMap poll_callbacks_;

  // Records the last time at which sound was audible from any stream.
  base::TimeTicks last_blurt_time_;

  // Set to true if the last call to MaybeToggle() determined the indicator
  // should be turned on.
  bool was_recently_audible_;

  // Whether the WebContents is currently audible.
  bool is_audible_;

  // Calls Poll() at regular intervals while |poll_callbacks_| is non-empty.
  base::RepeatingTimer poll_timer_;

  // Started only when an indicator is toggled on, to turn it off again in the
  // future.
  base::OneShotTimer off_timer_;

  DISALLOW_COPY_AND_ASSIGN(AudioStreamMonitor);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_AUDIO_STREAM_MONITOR_H_
