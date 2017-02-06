// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/chunk_stream.h"

#include <stddef.h>
#include <string.h>

#define __STDC_LIMIT_MACROS
#ifdef _WIN32
#include <limits.h>
#else
#include <stdint.h>
#endif

#include <algorithm>

namespace chrome_pdf {

ChunkStream::ChunkStream() : stream_size_(0) {
}

ChunkStream::~ChunkStream() {
}

void ChunkStream::Clear() {
  chunks_.clear();
  data_.clear();
  stream_size_ = 0;
}

void ChunkStream::Preallocate(size_t stream_size) {
  data_.reserve(stream_size);
  stream_size_ = stream_size;
}

size_t ChunkStream::GetSize() const {
  return data_.size();
}

bool ChunkStream::WriteData(size_t offset, void* buffer, size_t size) {
  if (SIZE_MAX - size < offset)
    return false;

  if (data_.size() < offset + size)
    data_.resize(offset + size);

  memcpy(&data_[offset], buffer, size);

  if (chunks_.empty()) {
    chunks_[offset] = size;
    return true;
  }

  std::map<size_t, size_t>::iterator start = chunks_.upper_bound(offset);
  if (start != chunks_.begin())
    --start;  // start now points to the key equal or lower than offset.
  if (start->first + start->second < offset)
    ++start;  // start element is entirely before current chunk, skip it.

  std::map<size_t, size_t>::iterator end = chunks_.upper_bound(offset + size);
  if (start == end) {  // No chunks to merge.
    chunks_[offset] = size;
    return true;
  }

  --end;

  size_t new_offset = std::min<size_t>(start->first, offset);
  size_t new_size =
      std::max<size_t>(end->first + end->second, offset + size) - new_offset;

  chunks_.erase(start, ++end);

  chunks_[new_offset] = new_size;

  return true;
}

bool ChunkStream::ReadData(size_t offset, size_t size, void* buffer) const {
  if (!IsRangeAvailable(offset, size))
    return false;

  memcpy(buffer, &data_[offset], size);
  return true;
}

bool ChunkStream::GetMissedRanges(
    size_t offset, size_t size,
    std::vector<std::pair<size_t, size_t> >* ranges) const {
  if (IsRangeAvailable(offset, size))
    return false;

  ranges->clear();
  if (chunks_.empty()) {
    ranges->push_back(std::pair<size_t, size_t>(offset, size));
    return true;
  }

  std::map<size_t, size_t>::const_iterator start = chunks_.upper_bound(offset);
  if (start != chunks_.begin())
    --start;  // start now points to the key equal or lower than offset.
  if (start->first + start->second < offset)
    ++start;  // start element is entirely before current chunk, skip it.

  std::map<size_t, size_t>::const_iterator end =
      chunks_.upper_bound(offset + size);
  if (start == end) {  // No data in the current range available.
    ranges->push_back(std::pair<size_t, size_t>(offset, size));
    return true;
  }

  size_t cur_offset = offset;
  std::map<size_t, size_t>::const_iterator it;
  for (it = start; it != end; ++it) {
    if (cur_offset < it->first) {
      size_t new_size = it->first - cur_offset;
      ranges->push_back(std::pair<size_t, size_t>(cur_offset, new_size));
      cur_offset = it->first + it->second;
    } else if (cur_offset < it->first + it->second) {
      cur_offset = it->first + it->second;
    }
  }

  // Add last chunk.
  if (cur_offset < offset + size)
    ranges->push_back(std::pair<size_t, size_t>(cur_offset,
        offset + size - cur_offset));

  return true;
}

bool ChunkStream::IsRangeAvailable(size_t offset, size_t size) const {
  if (chunks_.empty())
    return false;

  if (SIZE_MAX - size < offset)
    return false;

  std::map<size_t, size_t>::const_iterator it = chunks_.upper_bound(offset);
  if (it == chunks_.begin())
    return false;  // No chunks includes offset byte.

  --it;  // Now it starts equal or before offset.
  return (it->first + it->second) >= (offset + size);
}

size_t ChunkStream::GetFirstMissingByte() const {
  if (chunks_.empty())
    return 0;
  std::map<size_t, size_t>::const_iterator begin = chunks_.begin();
  return begin->first > 0 ? 0 : begin->second;
}

size_t ChunkStream::GetFirstMissingByteInInterval(size_t offset) const {
  if (chunks_.empty())
    return 0;
  std::map<size_t, size_t>::const_iterator it = chunks_.upper_bound(offset);
  if (it == chunks_.begin())
    return 0;
  --it;
  return it->first + it->second;
}

size_t ChunkStream::GetLastMissingByteInInterval(size_t offset) const {
  if (chunks_.empty())
    return stream_size_ - 1;
  std::map<size_t, size_t>::const_iterator it = chunks_.upper_bound(offset);
  if (it == chunks_.end())
    return stream_size_ - 1;
  return it->first - 1;
}

}  // namespace chrome_pdf
