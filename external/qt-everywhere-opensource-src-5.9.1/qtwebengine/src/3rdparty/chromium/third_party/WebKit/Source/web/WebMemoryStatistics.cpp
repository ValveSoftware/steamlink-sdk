// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/web/WebMemoryStatistics.h"

#include "platform/heap/Heap.h"
#include "wtf/allocator/Partitions.h"

namespace blink {

namespace {

class LightPartitionStatsDumperImpl : public WTF::PartitionStatsDumper {
 public:
  LightPartitionStatsDumperImpl() : m_totalActiveBytes(0) {}

  void partitionDumpTotals(
      const char* partitionName,
      const WTF::PartitionMemoryStats* memoryStats) override {
    m_totalActiveBytes += memoryStats->totalActiveBytes;
  }

  void partitionsDumpBucketStats(
      const char* partitionName,
      const WTF::PartitionBucketMemoryStats*) override {}

  size_t totalActiveBytes() const { return m_totalActiveBytes; }

 private:
  size_t m_totalActiveBytes;
};

}  // namespace

WebMemoryStatistics WebMemoryStatistics::Get() {
  LightPartitionStatsDumperImpl dumper;
  WebMemoryStatistics statistics;

  WTF::Partitions::dumpMemoryStats(true, &dumper);
  statistics.partitionAllocTotalAllocatedBytes = dumper.totalActiveBytes();

  statistics.blinkGCTotalAllocatedBytes =
      ProcessHeap::totalAllocatedObjectSize() +
      ProcessHeap::totalMarkedObjectSize();
  return statistics;
}

}  // namespace blink
