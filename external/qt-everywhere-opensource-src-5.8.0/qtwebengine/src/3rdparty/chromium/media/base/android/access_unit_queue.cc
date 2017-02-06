// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/access_unit_queue.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "media/base/demuxer_stream.h"

namespace media {

namespace {
// Amount of history chunks we keep by default. The zero size means we do not
// keep chunks before the current one and the history is limited by the size
// of one chunk.
const int kDefaultHistoryChunksAmount = 0;
}

AccessUnitQueue::AccessUnitQueue()
    : index_in_chunk_(0),
      history_chunks_amount_(kDefaultHistoryChunksAmount),
      has_eos_(false) {
  current_chunk_ = chunks_.end();
}

AccessUnitQueue::~AccessUnitQueue() {
  STLDeleteContainerPointers(chunks_.begin(), chunks_.end());
}

void AccessUnitQueue::PushBack(const DemuxerData& data) {
  // Media thread
  DCHECK(!data.access_units.empty());

#if DCHECK_IS_ON()
  // If there is an AU with |kConfigChanged| status, it must be the last
  // AU in the chunk and the data should have exactly one corresponding
  // DemuxerConfigs.
  for (size_t i = 0; i < data.access_units.size(); ++i) {
    const AccessUnit& unit = data.access_units[i];

    // EOS must be the last unit in the chunk.
    if (unit.is_end_of_stream) {
      DCHECK(i == data.access_units.size() - 1);
    }

    // kConfigChanged must be the last unit in the chunk.
    if (unit.status == DemuxerStream::kConfigChanged) {
      DCHECK(i == data.access_units.size() - 1);
      DCHECK(data.demuxer_configs.size() == 1);
    }

    if (unit.status == DemuxerStream::kAborted) {
      DVLOG(1) << "AccessUnitQueue::" << __FUNCTION__ << " kAborted";
    }
  }
#endif

  // Create the next chunk and copy data to it.
  DemuxerData* chunk = new DemuxerData(data);

  // EOS flag can only be in the last access unit.
  bool has_eos = chunk->access_units.back().is_end_of_stream;

  // Append this chunk to the queue.
  base::AutoLock lock(lock_);

  // Ignore the input after we have received EOS.
  if (has_eos_) {
    delete chunk;
    return;
  }

  bool was_empty = (current_chunk_ == chunks_.end());

  // The container |chunks_| will own the chunk.
  chunks_.push_back(chunk);

  // Position the current chunk.
  if (was_empty) {
    current_chunk_ = --chunks_.end();
    index_in_chunk_ = 0;
  }

  // We expect that the chunk containing EOS is the last chunk.
  DCHECK(!has_eos_);
  has_eos_ = has_eos;
}

void AccessUnitQueue::Advance() {
  // Decoder thread
  base::AutoLock lock(lock_);

  if (current_chunk_ == chunks_.end())
    return;

  ++index_in_chunk_;
  if (index_in_chunk_ < (*current_chunk_)->access_units.size())
    return;

  index_in_chunk_ = 0;
  ++current_chunk_;

  // Keep only |history_chunks_amount_| before the current one.
  // std::distance() and std::advance() do not work efficiently with std::list,
  // but the history_size should be small (default is 0).
  size_t num_consumed_chunks = std::distance(chunks_.begin(), current_chunk_);
  if (num_consumed_chunks > history_chunks_amount_) {
    DataChunkQueue::iterator first_to_keep = chunks_.begin();
    std::advance(first_to_keep, num_consumed_chunks - history_chunks_amount_);
    STLDeleteContainerPointers(chunks_.begin(), first_to_keep);
    chunks_.erase(chunks_.begin(), first_to_keep);
  }
}

void AccessUnitQueue::Flush() {
  // Media thread
  base::AutoLock lock(lock_);

  STLDeleteContainerPointers(chunks_.begin(), chunks_.end());
  chunks_.clear();

  current_chunk_ = chunks_.end();
  index_in_chunk_ = 0;
  has_eos_ = false;
}

bool AccessUnitQueue::RewindToLastKeyFrame() {
  // Media thread
  base::AutoLock lock(lock_);

  // Search for the key frame backwards. Start with the current AU.

  // Start with current chunk.
  if (current_chunk_ != chunks_.end()) {
    for (int i = (int)index_in_chunk_; i >= 0; --i) {
      if ((*current_chunk_)->access_units[i].is_key_frame) {
        index_in_chunk_ = i;
        return true;
      }
    }
  }

  // Position reverse iterator before the current chunk.
  DataChunkQueue::reverse_iterator rchunk(current_chunk_);

  for (; rchunk != chunks_.rend(); ++rchunk) {
    int i = (int)(*rchunk)->access_units.size() - 1;
    for (; i >= 0; --i) {
      if ((*rchunk)->access_units[i].is_key_frame) {
        index_in_chunk_ = i;
        current_chunk_ = --rchunk.base();
        return true;
      }
    }
  }

  return false;
}

AccessUnitQueue::Info AccessUnitQueue::GetInfo() const {
  // Media thread, Decoder thread

  Info info;
  base::AutoLock lock(lock_);

  GetUnconsumedAccessUnitLength(&info.length, &info.data_length);

  info.has_eos = has_eos_;
  info.front_unit = nullptr;
  info.configs = nullptr;

  if (info.length > 0) {
    DCHECK(current_chunk_ != chunks_.end());
    DCHECK(index_in_chunk_ < (*current_chunk_)->access_units.size());
    info.front_unit = &(*current_chunk_)->access_units[index_in_chunk_];

    if (info.front_unit->status == DemuxerStream::kConfigChanged) {
      DCHECK((*current_chunk_)->demuxer_configs.size() == 1);
      info.configs = &(*current_chunk_)->demuxer_configs[0];
    }
  }
  return info;
}

void AccessUnitQueue::SetHistorySizeForTesting(size_t history_chunks_amount) {
  history_chunks_amount_ = history_chunks_amount;
}

void AccessUnitQueue::GetUnconsumedAccessUnitLength(int* total_length,
                                                    int* data_length) const {
  *total_length = *data_length = 0;

  DataChunkQueue::const_iterator chunk;
  for (chunk = current_chunk_; chunk != chunks_.end(); ++chunk) {
    size_t chunk_size = (*chunk)->access_units.size();
    *total_length += chunk_size;
    *data_length += chunk_size;

    // Do not count configuration changes for |data_length|.
    if (!(*chunk)->demuxer_configs.empty()) {
      DCHECK((*chunk)->demuxer_configs.size() == 1);
      --(*data_length);
    }
  }

  *total_length -= index_in_chunk_;
  *data_length -= index_in_chunk_;
}

}  // namespace media
