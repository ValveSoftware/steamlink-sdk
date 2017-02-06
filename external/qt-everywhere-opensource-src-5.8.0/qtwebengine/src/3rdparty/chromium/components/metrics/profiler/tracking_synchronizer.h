// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PROFILER_TRACKING_SYNCHRONIZER_H_
#define COMPONENTS_METRICS_PROFILER_TRACKING_SYNCHRONIZER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/tracked_objects.h"
#include "components/metrics/profiler/tracking_synchronizer_delegate.h"
#include "components/metrics/proto/chrome_user_metrics_extension.pb.h"

// This class maintains state that is used to upload profiler data from the
// various processes, into the browser process. Such transactions are usually
// instigated by the browser. In general, a process will respond by gathering
// profiler data, and transmitting the pickled profiler data. We collect the
// data in asynchronous mode that doesn't block the UI thread.
//
// To assure that all the processes have responded, a counter is maintained
// to indicate the number of pending (not yet responsive) processes. We tag
// each group of requests with a sequence number. For each group of requests, we
// create RequestContext object which stores the sequence number, pending
// processes and the callback_object that needs to be notified when we receive
// an update from processes. When an update arrives we find the RequestContext
// associated with sequence number and send the unpickled profiler data to the
// |callback_object_|.

namespace metrics {

class TrackingSynchronizerObserver;

typedef base::Callback<std::unique_ptr<TrackingSynchronizerDelegate>(
    TrackingSynchronizer*)>
    TrackingSynchronizerDelegateFactory;

class TrackingSynchronizer
    : public base::RefCountedThreadSafe<TrackingSynchronizer> {
 public:
  // Construction also sets up the global singleton instance. This instance is
  // used to communicate between the IO and UI thread, and is destroyed only as
  // the main thread (browser_main) terminates, which means the IO thread has
  // already completed, and will not need this instance any further.
  // |clock| is a clock used for durations of profiling phases.
  // |delegate| is used to abstract platform-specific profiling functionality.
  TrackingSynchronizer(
      std::unique_ptr<base::TickClock> clock,
      const TrackingSynchronizerDelegateFactory& delegate_factory);

  // Contact all processes, and get them to upload to the browser any/all
  // changes to profiler data. It calls |callback_object|'s SetData method with
  // the data received from each sub-process.
  // This method is accessible on the UI thread.
  static void FetchProfilerDataAsynchronously(
      const base::WeakPtr<TrackingSynchronizerObserver>& callback_object);

  // Called when a profiling phase completes. |profiling_event| is the event
  // that triggered the completion of the current phase, and begins a new phase.
  // This method is accessible on the UI thread.
  static void OnProfilingPhaseCompleted(
      ProfilerEventProto::ProfilerEvent profiling_event);

  // Send profiler_data back to |callback_object_| by calling
  // DecrementPendingProcessesAndSendData which records that we are waiting
  // for one less profiler data from renderer or browser child process for the
  // given sequence number. This method is accessible on UI thread.
  void OnProfilerDataCollected(
      int sequence_number,
      const tracked_objects::ProcessDataSnapshot& profiler_data,
      ProfilerEventProto::TrackedObject::ProcessType process_type);

  // Update the number of pending processes for the given |sequence_number|.
  // This is called on UI thread.
  void OnPendingProcesses(int sequence_number, int pending_processes, bool end);

 protected:
  virtual ~TrackingSynchronizer();

  // Update the sequence of completed phases with a new phase completion info.
  void RegisterPhaseCompletion(
      ProfilerEventProto::ProfilerEvent profiling_event);

  // Notify |observer| about |profiler_data| received from process of type
  // |process_type|.
  void SendData(const tracked_objects::ProcessDataSnapshot& profiler_data,
                ProfilerEventProto::TrackedObject::ProcessType process_type,
                TrackingSynchronizerObserver* observer) const;

 private:
  friend class base::RefCountedThreadSafe<TrackingSynchronizer>;

  class RequestContext;

  // Establish a new sequence_number_, and use it to notify all the processes of
  // the need to supply, to the browser, their tracking data. It also registers
  // |callback_object| in |outstanding_requests_| map. Return the
  // sequence_number_ that was used. This method is accessible on UI thread.
  int RegisterAndNotifyAllProcesses(
      const base::WeakPtr<TrackingSynchronizerObserver>& callback_object);

  // Notifies all processes of a completion of a profiling phase.
  // |profiling_event| is the event associated with the phase change.
  void NotifyAllProcessesOfProfilingPhaseCompletion(
      ProfilerEventProto::ProfilerEvent profiling_event);

  // It finds the RequestContext for the given |sequence_number| and notifies
  // the RequestContext's |callback_object_| about the |value|. This is called
  // whenever we receive profiler data from processes. It also records that we
  // are waiting for one less profiler data from a process for the given
  // sequence number. If we have received a response from all renderers and
  // browser processes, then it calls RequestContext's DeleteIfAllDone to delete
  // the entry for sequence_number. This method is accessible on UI thread.
  void DecrementPendingProcessesAndSendData(
      int sequence_number,
      const tracked_objects::ProcessDataSnapshot& profiler_data,
      ProfilerEventProto::TrackedObject::ProcessType process_type);

  // Get a new sequence number to be sent to processes from browser process.
  // This method is accessible on UI thread.
  int GetNextAvailableSequenceNumber();

  // Used to verify that certain methods are called on the UI thread (the thread
  // on which this object was created).
  base::ThreadChecker thread_checker_;

  // We don't track the actual processes that are contacted for an update, only
  // the count of the number of processes, and we can sometimes time-out and
  // give up on a "slow to respond" process.  We use a sequence_number to be
  // sure a response from a process is associated with the current round of
  // requests. All sequence numbers used are non-negative.
  // last_used_sequence_number_ is the most recently used number (used to avoid
  // reuse for a long time).
  int last_used_sequence_number_;

  // Sequence of events associated with already completed profiling phases. The
  // index in the vector is the phase number. The current phase is not included.
  std::vector<ProfilerEventProto::ProfilerEvent>
      phase_completion_events_sequence_;

  // Clock for profiling phase durations.
  const std::unique_ptr<base::TickClock> clock_;

  // Times of starts of all profiling phases, including the current phase. The
  // index in the vector is the phase number.
  std::vector<base::TimeTicks> phase_start_times_;

  // This object's delegate.
  // NOTE: Leave this ivar last so that the delegate is torn down first at
  // destruction, as it has a reference to this object.
  std::unique_ptr<TrackingSynchronizerDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(TrackingSynchronizer);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_PROFILER_TRACKING_SYNCHRONIZER_H_
