# Heap Profiler Internals

This document describes how the heap profiler works and how to add heap
profiling support to your allocator. If you just want to know how to use it,
see [Heap Profiling with MemoryInfra](heap_profiler.md)

[TOC]

## Overview

The heap profiler consists of tree main components:

 * **The Context Tracker**: Responsible for providing context (pseudo stack
   backtrace) when an allocation occurs.
 * **The Allocation Register**: A specialized hash table that stores allocation
   details by address.
 * **The Heap Dump Writer**: Extracts the most important information from a set
   of recorded allocations and converts it into a format that can be dumped into
   the trace log.

These components are designed to work well together, but to be usable
independently as well.

When there is a way to get notified of all allocations and frees, this is the
normal flow:

 1. When an allocation occurs, call
    [`AllocationContextTracker::GetInstanceForCurrentThread()->GetContextSnapshot()`][context-tracker]
    to get an [`AllocationContext`][alloc-context].
 2. Insert that context together with the address and size into an
    [`AllocationRegister`][alloc-register] by calling `Insert()`.
 3. When memory is freed, remove it from the register with `Remove()`.
 4. On memory dump, collect the allocations from the register, call
    [`ExportHeapDump()`][export-heap-dump], and add the generated heap dump to
    the memory dump.

[context-tracker]:  https://chromium.googlesource.com/chromium/src/+/master/base/trace_event/heap_profiler_allocation_context_tracker.h
[alloc-context]:    https://chromium.googlesource.com/chromium/src/+/master/base/trace_event/heap_profiler_allocation_context.h
[alloc-register]:   https://chromium.googlesource.com/chromium/src/+/master/base/trace_event/heap_profiler_allocation_register.h
[export-heap-dump]: https://chromium.googlesource.com/chromium/src/+/master/base/trace_event/heap_profiler_heap_dump_writer.h

*** aside
An allocator can skip step 2 and 3 if it is able to store the context itself,
and if it is able to enumerate all allocations for step 4.
***

When heap profiling is enabled (the `--enable-heap-profiling` flag is passed),
the memory dump manager calls `OnHeapProfilingEnabled()` on every
`MemoryDumpProvider` as early as possible, so allocators can start recording
allocations. This should be done even when tracing has not been started,
because these allocations might still be around when a heap dump happens during
tracing.

## Context Tracker

The [`AllocationContextTracker`][context-tracker] is a thread-local object. Its
main purpose is to keep track of a pseudo stack of trace events. Chrome has
been instrumented with lots of `TRACE_EVENT` macros. These trace events push
their name to a thread-local stack when they go into scope, and pop when they
go out of scope, if all of the following conditions have been met:

 * A trace is being recorded.
 * The category of the event is enabled in the trace config.
 * Heap profiling is enabled (with the `--enable-heap-profiling` flag).

This means that allocations that occur before tracing is started will not have
backtrace information in their context.

A thread-local instance of the context tracker is initialized lazily when it is
first accessed. This might be because a trace event pushed or popped, or because
`GetContextSnapshot()` was called when an allocation occurred.

[`AllocationContext`][alloc-context] is what is used to group and break down
allocations. Currently `AllocationContext` has the following fields:

 * Backtrace: filled by the context tracker, obtained from the thread-local
   pseudo stack.
 * Type name: to be filled in at a point where the type of a pointer is known,
   set to _[unknown]_ by default.

It is possible to modify this context after insertion into the register, for
instance to set the type name if it was not known at the time of allocation.

## Allocation Register

The [`AllocationRegister`][alloc-register] is a hash table specialized for
storing `(size, AllocationContext)` pairs by address. It has been optimized for
Chrome's typical number of unfreed allocations, and it is backed by `mmap`
memory directly so there are no reentrancy issues when using it to record
`malloc` allocations.

The allocation register is threading-agnostic. Access must be synchronised
properly.

## Heap Dump Writer

Dumping every single allocation in the allocation register straight into the
trace log is not an option due to the sheer volume (~300k unfreed allocations).
The role of the [`ExportHeapDump()`][export-heap-dump] function is to group
allocations, striking a balance between trace log size and detail.

See the [Heap Dump Format][heap-dump-format] document for more details about the
structure of the heap dump in the trace log.

[heap-dump-format]: https://docs.google.com/document/d/1NqBg1MzVnuMsnvV1AKLdKaPSPGpd81NaMPVk5stYanQ

## Instrumenting an Allocator

Below is an example of adding heap profiling support to an allocator that has
an existing memory dump provider.

```cpp
class FooDumpProvider : public MemoryDumpProvider {

  // Kept as pointer because |AllocationRegister| allocates a lot of virtual
  // address space when constructed, so only construct it when heap profiling is
  // enabled.
  scoped_ptr<AllocationRegister> allocation_register_;
  Lock allocation_register_lock_;

  static FooDumpProvider* GetInstance();

  void InsertAllocation(void* address, size_t size) {
    AllocationContext context = AllocationContextTracker::GetInstanceForCurrentThread()->GetContextSnapshot();
    AutoLock lock(allocation_register_lock_);
    allocation_register_->Insert(address, size, context);
  }

  void RemoveAllocation(void* address) {
    AutoLock lock(allocation_register_lock_);
    allocation_register_->Remove(address);
  }

  // Will be called as early as possible by the memory dump manager.
  void OnHeapProfilingEnabled(bool enabled) override {
    AutoLock lock(allocation_register_lock_);
    allocation_register_.reset(new AllocationRegister());

    // At this point, make sure that from now on, for every allocation and
    // free, |FooDumpProvider::GetInstance()->InsertAllocation()| and
    // |RemoveAllocation| are called.
  }

  bool OnMemoryDump(const MemoryDumpArgs& args,
                    ProcessMemoryDump& pmd) override {
    // Do regular dumping here.

    // Dump the heap only for detailed dumps.
    if (args.level_of_detail == MemoryDumpLevelOfDetail::DETAILED) {
      TraceEventMemoryOverhead overhead;
      hash_map<AllocationContext, size_t> bytes_by_context;

      {
        AutoLock lock(allocation_register_lock_);
        if (allocation_register_) {
          // Group allocations in the register into |bytes_by_context|, but do
          // no additional processing inside the lock.
          for (const auto& alloc_size : *allocation_register_)
            bytes_by_context[alloc_size.context] += alloc_size.size;

          allocation_register_->EstimateTraceMemoryOverhead(&overhead);
        }
      }

      if (!bytes_by_context.empty()) {
        scoped_refptr<TracedValue> heap_dump = ExportHeapDump(
            bytes_by_context,
            pmd->session_state()->stack_frame_deduplicator(),
            pmb->session_state()->type_name_deduplicator());
        pmd->AddHeapDump("foo_allocator", heap_dump);
        overhead.DumpInto("tracing/heap_profiler", pmd);
      }
    }

    return true;
  }
};

```

*** aside
The implementation for `malloc` is more complicated because it needs to deal
with reentrancy.
***
