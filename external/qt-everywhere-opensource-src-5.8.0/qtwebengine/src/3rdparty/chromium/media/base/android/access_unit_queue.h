// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_ACCESS_UNIT_QUEUE_H_
#define MEDIA_BASE_ANDROID_ACCESS_UNIT_QUEUE_H_

#include <stddef.h>

#include <list>

#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "media/base/android/demuxer_stream_player_params.h"

namespace media {

// The queue of incoming data for MediaCodecDecoder.
//
// The data comes in the form of access units. Each access unit has a type.
// If the type is |kConfigChanged| the access unit itself has no data, but
// is accompanied with DemuxerConfigs.
// The queue should be accessed on the Media thread that puts the incoming data
// in and on the Decoder thread that gets the next access unit and eventually
// removes it from the queue.
class AccessUnitQueue {
 public:
  // Information about the queue state and the access unit at the front.
  struct Info {
    // The unit at front. Null if the queue is empty. This pointer may be
    // invalidated by the next Advance() or Flush() call and must be used
    // before the caller calls these methods. The |front_unit| is owned by
    // the queue itself - never delete it through this pointer.
    const AccessUnit* front_unit;

    // Configs for the front unit if it is |kConfigChanged|, null otherwise.
    // The same validity rule applies: this pointer is only valid till the next
    // Advance() or Flush() call, and |configs| is owned by the queue itself.
    const DemuxerConfigs* configs;

    // Number of access units in the queue.
    int length;

    // Number of access units in the queue excluding config units.
    int data_length;

    // Whether End Of Stream has been added to the queue. Cleared by Flush().
    bool has_eos;

    Info() : front_unit(nullptr), configs(nullptr), length(0), has_eos(false) {}
  };

  AccessUnitQueue();
  ~AccessUnitQueue();

  // Appends the incoming data to the queue.
  void PushBack(const DemuxerData& frames);

  // Advances the front position to next unit. Logically the preceding units
  // do not exist, but they can be physically removed later.
  void Advance();

  // Clears the queue, resets the length to zero and clears EOS condition.
  void Flush();

  // Looks back for the first key frame starting from the current one (i.e.
  // the look-back is inclusive of the current front position).
  // If the key frame exists, sets the current access unit to it and returns
  // true. Otherwise returns false.
  bool RewindToLastKeyFrame();

  // Returns the information about the queue.
  // The result is invalidated by the following Advance() or Flush call.
  // There must be only one |Info| consumer at a time.
  Info GetInfo() const;

  // For unit tests only. These methods are not thread safe.
  size_t NumChunksForTesting() const { return chunks_.size(); }
  void SetHistorySizeForTesting(size_t number_of_history_chunks);

 private:
  // Returns the total number of access units (total_length) and the number of
  // units excluding configiration change requests (data_length). The number is
  // calculated between the current one and the end, incuding the current.
  // Logically these are units that have not been consumed.
  void GetUnconsumedAccessUnitLength(int* total_length, int* data_length) const;

  // The queue of data chunks. It owns the chunks.
  typedef std::list<DemuxerData*> DataChunkQueue;
  DataChunkQueue chunks_;

  // The chunk that contains the current access unit.
  DataChunkQueue::iterator current_chunk_;

  // Index of the current access unit within the current chunk.
  size_t index_in_chunk_;

  // Amount of chunks before the |current_chunk_| that's kept for history.
  size_t history_chunks_amount_;

  // Indicates that a unit with End Of Stream flag has been appended.
  bool has_eos_;

  // The lock protects all fields together.
  mutable base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(AccessUnitQueue);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_ACCESS_UNIT_QUEUE_H_
