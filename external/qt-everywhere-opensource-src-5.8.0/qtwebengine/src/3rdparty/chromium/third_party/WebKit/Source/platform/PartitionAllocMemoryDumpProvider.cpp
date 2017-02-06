// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/PartitionAllocMemoryDumpProvider.h"

#include "base/strings/stringprintf.h"
#include "base/trace_event/heap_profiler_allocation_context.h"
#include "base/trace_event/heap_profiler_allocation_context_tracker.h"
#include "base/trace_event/heap_profiler_allocation_register.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event_memory_overhead.h"
#include "wtf/allocator/Partitions.h"
#include "wtf/text/WTFString.h"

namespace blink {

namespace {

using namespace WTF;

void reportAllocation(void* address, size_t size, const char* typeName)
{
    PartitionAllocMemoryDumpProvider::instance()->insert(address, size, typeName);
}

void reportFree(void* address)
{
    PartitionAllocMemoryDumpProvider::instance()->remove(address);
}

const char kPartitionAllocDumpName[] = "partition_alloc";
const char kPartitionsDumpName[] = "partitions";

std::string getPartitionDumpName(const char* partitionName)
{
    return base::StringPrintf("%s/%s/%s", kPartitionAllocDumpName, kPartitionsDumpName, partitionName);
}

// This class is used to invert the dependency of PartitionAlloc on the
// PartitionAllocMemoryDumpProvider. This implements an interface that will
// be called with memory statistics for each bucket in the allocator.
class PartitionStatsDumperImpl final : public PartitionStatsDumper {
    DISALLOW_NEW();
    WTF_MAKE_NONCOPYABLE(PartitionStatsDumperImpl);
public:
    PartitionStatsDumperImpl(base::trace_event::ProcessMemoryDump* memoryDump, base::trace_event::MemoryDumpLevelOfDetail levelOfDetail)
        : m_memoryDump(memoryDump)
        , m_uid(0)
        , m_totalActiveBytes(0)
    {
    }

    // PartitionStatsDumper implementation.
    void partitionDumpTotals(const char* partitionName, const PartitionMemoryStats*) override;
    void partitionsDumpBucketStats(const char* partitionName, const PartitionBucketMemoryStats*) override;

    size_t totalActiveBytes() const { return m_totalActiveBytes; }

private:
    base::trace_event::ProcessMemoryDump* m_memoryDump;
    unsigned long m_uid;
    size_t m_totalActiveBytes;
};

void PartitionStatsDumperImpl::partitionDumpTotals(const char* partitionName, const PartitionMemoryStats* memoryStats)
{
    m_totalActiveBytes += memoryStats->totalActiveBytes;
    std::string dumpName = getPartitionDumpName(partitionName);
    base::trace_event::MemoryAllocatorDump* allocatorDump = m_memoryDump->CreateAllocatorDump(dumpName);
    allocatorDump->AddScalar("size", "bytes", memoryStats->totalResidentBytes);
    allocatorDump->AddScalar("allocated_objects_size", "bytes", memoryStats->totalActiveBytes);
    allocatorDump->AddScalar("virtual_size", "bytes", memoryStats->totalMmappedBytes);
    allocatorDump->AddScalar("virtual_committed_size", "bytes", memoryStats->totalCommittedBytes);
    allocatorDump->AddScalar("decommittable_size", "bytes", memoryStats->totalDecommittableBytes);
    allocatorDump->AddScalar("discardable_size", "bytes", memoryStats->totalDiscardableBytes);
}

void PartitionStatsDumperImpl::partitionsDumpBucketStats(const char* partitionName, const PartitionBucketMemoryStats* memoryStats)
{
    ASSERT(memoryStats->isValid);
    std::string dumpName = getPartitionDumpName(partitionName);
    if (memoryStats->isDirectMap)
        dumpName.append(base::StringPrintf("/directMap_%lu", ++m_uid));
    else
        dumpName.append(base::StringPrintf("/bucket_%u", static_cast<unsigned>(memoryStats->bucketSlotSize)));

    base::trace_event::MemoryAllocatorDump* allocatorDump = m_memoryDump->CreateAllocatorDump(dumpName);
    allocatorDump->AddScalar("size", "bytes", memoryStats->residentBytes);
    allocatorDump->AddScalar("allocated_objects_size", "bytes", memoryStats->activeBytes);
    allocatorDump->AddScalar("slot_size", "bytes", memoryStats->bucketSlotSize);
    allocatorDump->AddScalar("decommittable_size", "bytes", memoryStats->decommittableBytes);
    allocatorDump->AddScalar("discardable_size", "bytes", memoryStats->discardableBytes);
    allocatorDump->AddScalar("total_pages_size", "bytes", memoryStats->allocatedPageSize);
    allocatorDump->AddScalar("active_pages", "objects", memoryStats->numActivePages);
    allocatorDump->AddScalar("full_pages", "objects", memoryStats->numFullPages);
    allocatorDump->AddScalar("empty_pages", "objects", memoryStats->numEmptyPages);
    allocatorDump->AddScalar("decommitted_pages", "objects", memoryStats->numDecommittedPages);
}

} // namespace

PartitionAllocMemoryDumpProvider* PartitionAllocMemoryDumpProvider::instance()
{
    DEFINE_STATIC_LOCAL(PartitionAllocMemoryDumpProvider, instance, ());
    return &instance;
}

bool PartitionAllocMemoryDumpProvider::OnMemoryDump(const base::trace_event::MemoryDumpArgs& args, base::trace_event::ProcessMemoryDump* memoryDump)
{
    using base::trace_event::MemoryDumpLevelOfDetail;

    MemoryDumpLevelOfDetail levelOfDetail = args.level_of_detail;
    if (m_isHeapProfilingEnabled) {
        // Overhead should always be reported, regardless of light vs. heavy.
        base::trace_event::TraceEventMemoryOverhead overhead;
        base::hash_map<base::trace_event::AllocationContext, base::trace_event::AllocationMetrics> metricsByContext;
        {
            MutexLocker locker(m_allocationRegisterMutex);
            // Dump only the overhead estimation in non-detailed dumps.
            if (levelOfDetail == MemoryDumpLevelOfDetail::DETAILED) {
                for (const auto& allocSize : *m_allocationRegister) {
                    base::trace_event::AllocationMetrics& metrics = metricsByContext[allocSize.context];
                    metrics.size += allocSize.size;
                    metrics.count++;
                }
            }
            m_allocationRegister->EstimateTraceMemoryOverhead(&overhead);
        }
        memoryDump->DumpHeapUsage(metricsByContext, overhead, "partition_alloc");
    }

    PartitionStatsDumperImpl partitionStatsDumper(memoryDump, levelOfDetail);

    base::trace_event::MemoryAllocatorDump* partitionsDump = memoryDump->CreateAllocatorDump(
        base::StringPrintf("%s/%s", kPartitionAllocDumpName, kPartitionsDumpName));

    // This method calls memoryStats.partitionsDumpBucketStats with memory statistics.
    WTF::Partitions::dumpMemoryStats(levelOfDetail != MemoryDumpLevelOfDetail::DETAILED, &partitionStatsDumper);

    base::trace_event::MemoryAllocatorDump* allocatedObjectsDump = memoryDump->CreateAllocatorDump(Partitions::kAllocatedObjectPoolName);
    allocatedObjectsDump->AddScalar("size", "bytes", partitionStatsDumper.totalActiveBytes());
    memoryDump->AddOwnershipEdge(allocatedObjectsDump->guid(), partitionsDump->guid());

    return true;
}

// |m_allocationRegister| should be initialized only when necessary to avoid waste of memory.
PartitionAllocMemoryDumpProvider::PartitionAllocMemoryDumpProvider()
    : m_allocationRegister(nullptr)
    , m_isHeapProfilingEnabled(false)
{
}

PartitionAllocMemoryDumpProvider::~PartitionAllocMemoryDumpProvider()
{
}

void PartitionAllocMemoryDumpProvider::OnHeapProfilingEnabled(bool enabled)
{
    if (enabled) {
        {
            MutexLocker locker(m_allocationRegisterMutex);
            if (!m_allocationRegister)
                m_allocationRegister.reset(new base::trace_event::AllocationRegister());
        }
        PartitionAllocHooks::setAllocationHook(reportAllocation);
        PartitionAllocHooks::setFreeHook(reportFree);
    } else {
        PartitionAllocHooks::setAllocationHook(nullptr);
        PartitionAllocHooks::setFreeHook(nullptr);
    }
    m_isHeapProfilingEnabled = enabled;
}

void PartitionAllocMemoryDumpProvider::insert(void* address, size_t size, const char* typeName)
{
    base::trace_event::AllocationContext context = base::trace_event::AllocationContextTracker::GetInstanceForCurrentThread()->GetContextSnapshot();
    context.type_name = typeName;
    MutexLocker locker(m_allocationRegisterMutex);
    if (m_allocationRegister)
        m_allocationRegister->Insert(address, size, context);
}

void PartitionAllocMemoryDumpProvider::remove(void* address)
{
    MutexLocker locker(m_allocationRegisterMutex);
    if (m_allocationRegister)
        m_allocationRegister->Remove(address);
}

} // namespace blink
