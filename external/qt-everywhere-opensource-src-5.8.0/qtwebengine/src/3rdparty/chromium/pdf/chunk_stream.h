// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_CHUNK_STREAM_H_
#define PDF_CHUNK_STREAM_H_

#include <stddef.h>

#include <map>
#include <utility>
#include <vector>

namespace chrome_pdf {

// This class collects a chunks of data into one data stream. Client can check
// if data in certain range is available, and get missing chunks of data.
class ChunkStream {
 public:
  ChunkStream();
  ~ChunkStream();

  void Clear();

  void Preallocate(size_t stream_size);
  size_t GetSize() const;

  bool WriteData(size_t offset, void* buffer, size_t size);
  bool ReadData(size_t offset, size_t size, void* buffer) const;

  // Returns vector of pairs where first is an offset, second is a size.
  bool GetMissedRanges(size_t offset, size_t size,
      std::vector<std::pair<size_t, size_t> >* ranges) const;
  bool IsRangeAvailable(size_t offset, size_t size) const;
  size_t GetFirstMissingByte() const;

  // Finds the first byte of the missing byte interval that offset belongs to.
  size_t GetFirstMissingByteInInterval(size_t offset) const;
  // Returns the last byte of the missing byte interval that offset belongs to.
  size_t GetLastMissingByteInInterval(size_t offset) const;

 private:
  std::vector<unsigned char> data_;

  // Pair, first - begining of the chunk, second - size of the chunk.
  std::map<size_t, size_t> chunks_;

  size_t stream_size_;
};

};  // namespace chrome_pdf

#endif  // PDF_CHUNK_STREAM_H_
