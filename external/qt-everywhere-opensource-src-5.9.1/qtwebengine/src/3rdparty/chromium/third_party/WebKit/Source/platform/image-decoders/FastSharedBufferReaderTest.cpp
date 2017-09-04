/*
 * Copyright (C) 2015 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/image-decoders/FastSharedBufferReader.h"
#include "platform/image-decoders/SegmentReader.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

const unsigned kDefaultTestSize = 4 * SharedBuffer::kSegmentSize;

void prepareReferenceData(char* buffer, size_t size) {
  for (size_t i = 0; i < size; ++i)
    buffer[i] = static_cast<char>(i);
}

PassRefPtr<SegmentReader> copyToROBufferSegmentReader(
    PassRefPtr<SegmentReader> input) {
  SkRWBuffer rwBuffer;
  const char* segment = 0;
  size_t position = 0;
  while (size_t length = input->getSomeData(segment, position)) {
    rwBuffer.append(segment, length);
    position += length;
  }
  return SegmentReader::createFromSkROBuffer(
      sk_sp<SkROBuffer>(rwBuffer.newRBufferSnapshot()));
}

PassRefPtr<SegmentReader> copyToDataSegmentReader(
    PassRefPtr<SegmentReader> input) {
  return SegmentReader::createFromSkData(input->getAsSkData());
}

struct SegmentReaders {
  RefPtr<SegmentReader> segmentReaders[3];

  SegmentReaders(PassRefPtr<SharedBuffer> input) {
    segmentReaders[0] = SegmentReader::createFromSharedBuffer(std::move(input));
    segmentReaders[1] = copyToROBufferSegmentReader(segmentReaders[0]);
    segmentReaders[2] = copyToDataSegmentReader(segmentReaders[0]);
  }
};

}  // namespace

TEST(FastSharedBufferReaderTest, nonSequentialReads) {
  char referenceData[kDefaultTestSize];
  prepareReferenceData(referenceData, sizeof(referenceData));
  RefPtr<SharedBuffer> data = SharedBuffer::create();
  data->append(referenceData, sizeof(referenceData));

  SegmentReaders readerStruct(data);
  for (auto segmentReader : readerStruct.segmentReaders) {
    FastSharedBufferReader reader(segmentReader);
    // Read size is prime such there will be a segment-spanning
    // read eventually.
    char tempBuffer[17];
    for (size_t dataPosition = 0;
         dataPosition + sizeof(tempBuffer) < sizeof(referenceData);
         dataPosition += sizeof(tempBuffer)) {
      const char* block = reader.getConsecutiveData(
          dataPosition, sizeof(tempBuffer), tempBuffer);
      ASSERT_FALSE(
          memcmp(block, referenceData + dataPosition, sizeof(tempBuffer)));
    }
  }
}

TEST(FastSharedBufferReaderTest, readBackwards) {
  char referenceData[kDefaultTestSize];
  prepareReferenceData(referenceData, sizeof(referenceData));
  RefPtr<SharedBuffer> data = SharedBuffer::create();
  data->append(referenceData, sizeof(referenceData));

  SegmentReaders readerStruct(data);
  for (auto segmentReader : readerStruct.segmentReaders) {
    FastSharedBufferReader reader(segmentReader);
    // Read size is prime such there will be a segment-spanning
    // read eventually.
    char tempBuffer[17];
    for (size_t dataOffset = sizeof(tempBuffer);
         dataOffset < sizeof(referenceData); dataOffset += sizeof(tempBuffer)) {
      const char* block = reader.getConsecutiveData(
          sizeof(referenceData) - dataOffset, sizeof(tempBuffer), tempBuffer);
      ASSERT_FALSE(memcmp(block,
                          referenceData + sizeof(referenceData) - dataOffset,
                          sizeof(tempBuffer)));
    }
  }
}

TEST(FastSharedBufferReaderTest, byteByByte) {
  char referenceData[kDefaultTestSize];
  prepareReferenceData(referenceData, sizeof(referenceData));
  RefPtr<SharedBuffer> data = SharedBuffer::create();
  data->append(referenceData, sizeof(referenceData));

  SegmentReaders readerStruct(data);
  for (auto segmentReader : readerStruct.segmentReaders) {
    FastSharedBufferReader reader(segmentReader);
    for (size_t i = 0; i < sizeof(referenceData); ++i) {
      ASSERT_EQ(referenceData[i], reader.getOneByte(i));
    }
  }
}

// Tests that a read from inside the penultimate segment to the very end of the
// buffer doesn't try to read off the end of the buffer.
TEST(FastSharedBufferReaderTest, readAllOverlappingLastSegmentBoundary) {
  const unsigned dataSize = 2 * SharedBuffer::kSegmentSize;
  char referenceData[dataSize];
  prepareReferenceData(referenceData, dataSize);
  RefPtr<SharedBuffer> data = SharedBuffer::create();
  data->append(referenceData, dataSize);

  SegmentReaders readerStruct(data);
  for (auto segmentReader : readerStruct.segmentReaders) {
    FastSharedBufferReader reader(segmentReader);
    char buffer[dataSize];
    reader.getConsecutiveData(0, dataSize, buffer);
    ASSERT_FALSE(memcmp(buffer, referenceData, dataSize));
  }
}

// Verify that reading past the end of the buffer does not break future reads.
TEST(SegmentReaderTest, readPastEndThenRead) {
  const unsigned dataSize = 2 * SharedBuffer::kSegmentSize;
  char referenceData[dataSize];
  prepareReferenceData(referenceData, dataSize);
  RefPtr<SharedBuffer> data = SharedBuffer::create();
  data->append(referenceData, dataSize);

  SegmentReaders readerStruct(data);
  for (auto segmentReader : readerStruct.segmentReaders) {
    const char* contents;
    size_t length = segmentReader->getSomeData(contents, dataSize);
    EXPECT_EQ(0u, length);

    length = segmentReader->getSomeData(contents, 0);
    EXPECT_LE(SharedBuffer::kSegmentSize, length);
  }
}

TEST(SegmentReaderTest, getAsSkData) {
  const unsigned dataSize = 4 * SharedBuffer::kSegmentSize;
  char referenceData[dataSize];
  prepareReferenceData(referenceData, dataSize);
  RefPtr<SharedBuffer> data = SharedBuffer::create();
  data->append(referenceData, dataSize);

  SegmentReaders readerStruct(data);
  for (auto segmentReader : readerStruct.segmentReaders) {
    sk_sp<SkData> skdata = segmentReader->getAsSkData();
    EXPECT_EQ(data->size(), skdata->size());

    const char* segment;
    size_t position = 0;
    for (size_t length = segmentReader->getSomeData(segment, position); length;
         length = segmentReader->getSomeData(segment, position)) {
      ASSERT_FALSE(memcmp(segment, skdata->bytes() + position, length));
      position += length;
    }
    EXPECT_EQ(position, dataSize);
  }
}

TEST(SegmentReaderTest, variableSegments) {
  const size_t dataSize = 3.5 * SharedBuffer::kSegmentSize;
  char referenceData[dataSize];
  prepareReferenceData(referenceData, dataSize);

  RefPtr<SegmentReader> segmentReader;
  {
    // Create a SegmentReader with difference sized segments, to test that
    // the SkROBuffer implementation works when two consecutive segments
    // are not the same size. This test relies on knowledge of the
    // internals of SkRWBuffer: it ensures that each segment is at least
    // 4096 (though the actual data may be smaller, if it has not been
    // written to yet), but when appending a larger amount it may create a
    // larger segment.
    SkRWBuffer rwBuffer;
    rwBuffer.append(referenceData, SharedBuffer::kSegmentSize);
    rwBuffer.append(referenceData + SharedBuffer::kSegmentSize,
                    2 * SharedBuffer::kSegmentSize);
    rwBuffer.append(referenceData + 3 * SharedBuffer::kSegmentSize,
                    .5 * SharedBuffer::kSegmentSize);

    segmentReader = SegmentReader::createFromSkROBuffer(
        sk_sp<SkROBuffer>(rwBuffer.newRBufferSnapshot()));
  }

  const char* segment;
  size_t position = 0;
  size_t lastLength = 0;
  for (size_t length = segmentReader->getSomeData(segment, position); length;
       length = segmentReader->getSomeData(segment, position)) {
    // It is not a bug to have consecutive segments of the same length, but
    // it does mean that the following test does not actually test what it
    // is intended to test.
    ASSERT_NE(length, lastLength);
    lastLength = length;

    ASSERT_FALSE(memcmp(segment, referenceData + position, length));
    position += length;
  }
  EXPECT_EQ(position, dataSize);
}

}  // namespace blink
