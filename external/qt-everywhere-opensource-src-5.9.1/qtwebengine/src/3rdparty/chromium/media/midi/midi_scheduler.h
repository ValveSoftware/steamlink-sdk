// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_MIDI_SCHEDULER_H_
#define MEDIA_MIDI_MIDI_SCHEDULER_H_

#include <stddef.h>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "media/midi/midi_export.h"

namespace midi {

class MidiManager;
class MidiManagerClient;

// TODO(crbug.com/467442): Make tasks cancelable per client.
class MIDI_EXPORT MidiScheduler final {
 public:
  // Both constructor and destructor should be run on the same thread. The
  // instance is bound to the TaskRunner of the constructing thread, on which
  // InvokeClosure() is run.
  explicit MidiScheduler(MidiManager* manager);
  ~MidiScheduler();

  // Post |closure| to |task_runner_| safely. The |closure| will not be invoked
  // after MidiScheduler is deleted. AccumulateMidiBytesSent() of |client| is
  // called internally. May be called on any thread, but the user should ensure
  // this method is not called on other threads during/after MidiScheduler
  // destruction.
  void PostSendDataTask(MidiManagerClient* client,
                        size_t length,
                        double timestamp,
                        const base::Closure& closure);

 private:
  void InvokeClosure(MidiManagerClient* client,
                     size_t length,
                     const base::Closure& closure);

  // MidiManager should own the MidiScheduler and be alive longer.
  MidiManager* manager_;

  // The TaskRunner of the thread on which the instance is constructed.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // Ensures |weak_factory_| is destructed, and WeakPtrs are dereferenced on the
  // same thread.
  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<MidiScheduler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MidiScheduler);
};

}  // namespace midi

#endif  // MEDIA_MIDI_MIDI_SCHEDULER_H_
