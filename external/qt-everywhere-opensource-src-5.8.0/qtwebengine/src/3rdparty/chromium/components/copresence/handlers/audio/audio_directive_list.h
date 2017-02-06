// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COPRESENCE_HANDLERS_AUDIO_AUDIO_DIRECTIVE_LIST_H_
#define COMPONENTS_COPRESENCE_HANDLERS_AUDIO_AUDIO_DIRECTIVE_LIST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "components/copresence/handlers/audio/tick_clock_ref_counted.h"
#include "components/copresence/proto/data.pb.h"

namespace media {
class AudioBusRefCounted;
}

namespace copresence {

class TickClockRefCounted;

struct AudioDirective final {
  // Default ctor, required by the priority queue.
  AudioDirective();
  AudioDirective(const std::string& op_id,
                 base::TimeTicks end_time,
                 const Directive& server_directive);

  std::string op_id;

  // We're currently using TimeTicks to track time. This may not work for cases
  // where your machine suspends. See crbug.com/426136
  base::TimeTicks end_time;

  Directive server_directive;
};

// This class maintains a list of active audio directives. It fetches the audio
// samples associated with a audio transmit directives and expires directives
// that have outlived their TTL.
// TODO(rkc): Once we implement more token technologies, move reusable code
// from here to a base class and inherit various XxxxDirectiveList
// classes from it.
class AudioDirectiveList final {
 public:
  explicit AudioDirectiveList(const scoped_refptr<TickClockRefCounted>& clock =
      make_scoped_refptr(new TickClockRefCounted(new base::DefaultTickClock)));
  ~AudioDirectiveList();

  void AddDirective(const std::string& op_id, const Directive& directive);
  void RemoveDirective(const std::string& op_id);

  std::unique_ptr<AudioDirective> GetActiveDirective();

  const std::vector<AudioDirective>& directives() const;

 private:
  // Comparator for comparing end_times on audio tokens.
  class LatestFirstComparator {
   public:
    // This will sort our queue with the 'latest' time being the top.
    bool operator()(const AudioDirective& left,
                    const AudioDirective& right) const {
      return left.end_time < right.end_time;
    }
  };

  std::vector<AudioDirective>::iterator FindDirectiveByOpId(
      const std::string& op_id);

  // This vector will be organized as a heap with the latest time as the first
  // element. Only currently active directives will exist in this list.
  std::vector<AudioDirective> active_directives_;

  scoped_refptr<TickClockRefCounted> clock_;

  DISALLOW_COPY_AND_ASSIGN(AudioDirectiveList);
};

}  // namespace copresence

#endif  // COMPONENTS_COPRESENCE_HANDLERS_AUDIO_AUDIO_DIRECTIVE_LIST_H_
