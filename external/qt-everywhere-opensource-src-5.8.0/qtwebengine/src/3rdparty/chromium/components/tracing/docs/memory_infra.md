# MemoryInfra

MemoryInfra is a timeline-based profiling system integrated in chrome://tracing.
It aims at creating Chrome-scale memory measurement tooling so that on any
Chrome in the world --- desktop, mobile, Chrome OS or any other --- with the
click of a button you can understand where memory is being used in your system.

[TOC]

## Getting Started

 1. Get a bleeding-edge or tip-of-tree build of Chrome.

 2. [Record a trace as usual][record-trace]: open [chrome://tracing][tracing]
    on Desktop Chrome or [chrome://inspect?tracing][inspect-tracing] to trace
    Chrome for Android.

 3. Make sure to enable the **memory-infra** category on the right.

      ![Tick the memory-infra checkbox when recording a trace.][memory-infra-box]

 4. For now, some subsystems only work if Chrome is started with the
    `--no-sandbox` flag. 
    <!-- TODO(primiano) TODO(ssid): https://crbug.com/461788 -->

[record-trace]:     https://sites.google.com/a/chromium.org/dev/developers/how-tos/trace-event-profiling-tool/recording-tracing-runs
[tracing]:          chrome://tracing
[inspect-tracing]:  chrome://inspect?tracing
[memory-infra-box]: https://storage.googleapis.com/chromium-docs.appspot.com/1c6d1886584e7cc6ffed0d377f32023f8da53e02

![Timeline View and Analysis View][tracing-views]

After recording a trace, you will see the **timeline view**. Timeline view
shows:

 * Total resident memory grouped by process (at the top).
 * Total resident memory grouped by subsystem (at the top).
 * Allocated memory per subsystem for every process.

Click one of the ![M][m-blue] dots to bring up the **analysis view**. Click
on a cell in analysis view to reveal more information about its subsystem.
PartitionAlloc for instance, has more details about its partitions.

![Component details for PartitionAlloc][partalloc-details]

The purple ![M][m-purple] dots represent heavy dumps. In these dumps, components
can provide more details than in the regular dumps. The full details of the
MemoryInfra UI are explained in its [design doc][mi-ui-doc].

[tracing-views]:     https://storage.googleapis.com/chromium-docs.appspot.com/db12015bd262385f0f8bd69133330978a99da1ca
[m-blue]:            https://storage.googleapis.com/chromium-docs.appspot.com/b60f342e38ff3a3767bbe4c8640d96a2d8bc864b
[partalloc-details]: https://storage.googleapis.com/chromium-docs.appspot.com/02eade61d57c83f8ef8227965513456555fc3324
[m-purple]:          https://storage.googleapis.com/chromium-docs.appspot.com/d7bdf4d16204c293688be2e5a0bcb2bf463dbbc3
[mi-ui-doc]:         https://docs.google.com/document/d/1b5BSBEd1oB-3zj_CBAQWiQZ0cmI0HmjmXG-5iNveLqw/edit

## Columns

**Columns in blue** reflect the amount of actual physical memory used by the
process. This is what exerts memory pressure on the system.

 * **Total Resident**: (TODO: document this).
 * **Peak Total Resident**: (TODO: document this).
 * **PSS**: (TODO: document this).
 * **Private Dirty**: (TODO: document this).
 * **Swapped**: (TODO: document this).

**Columns in black** reflect a best estimation of the the amount of physical
memory used by various subsystems of Chrome.

 * **Blink GC**: Memory used by [Oilpan][oilpan].
 * **CC**: Memory used by the compositor.
   See [cc/memory][cc-memory] for the full details.
 * **Discardable**: (TODO: document this).
 * **Font Caches**: (TODO: document this).
 * **GPU** and **GPU Memory Buffer**: GPU memory and RAM used for GPU purposes.
   See [GPU Memory Tracing][gpu-memory].
 * **LevelDB**: (TODO: document this).
 * **Malloc**: Memory allocated by calls to `malloc`, or `new` for most
     non-Blink objects.
 * **PartitionAlloc**: Memory allocated via [PartitionAlloc][partalloc].
   Blink objects that are not managed by Oilpan are allocated with
   PartitionAlloc.
 * **Skia**: (TODO: document this).
 * **SQLite**: (TODO: document this).
 * **V8**: (TODO: document this).
 * **Web Cache**: (TODO: document this).

The **tracing column in gray** reports memory that is used to collect all of the
above information. This memory would not be used if tracing were not enabled,
and it is discounted from malloc and the blue columns.

<!-- TODO(primiano): Improve this. https://crbug.com/??? -->

[oilpan]:     /third_party/WebKit/Source/platform/heap/BlinkGCDesign.md
[cc-memory]:  /cc/memory.md
[gpu-memory]: memory_infra_gpu.md
[partalloc]:  /third_party/WebKit/Source/wtf/PartitionAlloc.md

## Related Pages

 * [Adding MemoryInfra Tracing to a Component](adding_memory_infra_tracing.md)
 * [GPU Memory Tracing](memory_infra_gpu.md)
 * [Heap Profiler Internals](heap_profiler_internals.md)
 * [Heap Profiling with MemoryInfra](heap_profiler.md)
 * [Startup Tracing with MemoryInfra](memory_infra_startup_tracing.md)

## Rationale

Another memory profiler? What is wrong with tool X?
Most of the existing tools:

 * Are hard to get working with Chrome. (Massive symbols, require OS-specific
   tricks.)
 * Lack Chrome-related context.
 * Don't deal with multi-process scenarios.

MemoryInfra leverages the existing tracing infrastructure in Chrome and provides
contextual data:

 * **It speaks Chrome slang.**
   The Chromium codebase is instrumented. Its memory subsystems (allocators,
   caches, etc.) uniformly report their stats into the trace in a way that can
   be understood by Chrome developers. No more
   `__gnu_cxx::new_allocator< std::_Rb_tree_node< std::pair< std::string const, base::Value*>>> ::allocate`.
 * **Timeline data that can be correlated with other events.**
   Did memory suddenly increase during a specific Blink / V8 / HTML parsing
   event? Which subsystem increased? Did memory not go down as expected after
   closing a tab? Which other threads were active during a bloat?
 * **Works out of the box on desktop and mobile.**
   No recompilations with unmaintained `GYP_DEFINES`, no time-consuming
   symbolizations stages. All the logic is already into Chrome, ready to dump at
   any time.
 * **The same technology is used for telemetry and the ChromePerf dashboard.**
   See [the slides][chromeperf-slides] and take a look at
   [some ChromePerf dashboards][chromeperf] and
   [telemetry documentation][telemetry].

[chromeperf-slides]: https://docs.google.com/presentation/d/1OyxyT1sfg50lA36A7ibZ7-bBRXI1kVlvCW0W9qAmM_0/present?slide=id.gde150139b_0_137
[chromeperf]:        https://chromeperf.appspot.com/report?sid=3b54e60c9951656574e19252fadeca846813afe04453c98a49136af4c8820b8d
[telemetry]:         https://catapult.gsrc.io/telemetry

## Development

MemoryInfra is based on a simple and extensible architecture. See
[the slides][dp-slides] on how to get your subsystem reported in MemoryInfra,
or take a look at one of the existing examples such as
[malloc_dump_provider.cc][malloc-dp]. The crbug label is
[Hotlist-MemoryInfra][hotlist]. Don't hesitate to contact
[tracing@chromium.org][mailtracing] for questions and support.

[dp-slides]:   https://docs.google.com/presentation/d/1GI3HY3Mm5-Mvp6eZyVB0JiaJ-u3L1MMJeKHJg4lxjEI/present?slide=id.g995514d5c_1_45
[malloc-dp]:   https://chromium.googlesource.com/chromium/src.git/+/master/base/trace_event/malloc_dump_provider.cc
[hotlist]:     https://code.google.com/p/chromium/issues/list?q=label:Hotlist-MemoryInfra
[mailtracing]: mailto:tracing@chromium.org

## Design documents

Architectural:

<iframe width="100%" height="300px" src="https://docs.google.com/a/google.com/embeddedfolderview?id=0B3KuDeqD-lVJfmp0cW1VcE5XVWNxZndxelV5T19kT2NFSndYZlNFbkFpc3pSa2VDN0hlMm8">
</iframe>

Chrome-side design docs:

<iframe width="100%" height="300px" src="https://docs.google.com/a/google.com/embeddedfolderview?id=0B3KuDeqD-lVJfndSa2dleUQtMnZDeWpPZk1JV0QtbVM5STkwWms4YThzQ0pGTmU1QU9kNVk">
</iframe>

Catapult-side design docs:

<iframe width="100%" height="300px" src="https://docs.google.com/a/google.com/embeddedfolderview?id=0B3KuDeqD-lVJfm10bXd5YmRNWUpKOElOWS0xdU1tMmV1S3F4aHo0ZDJLTmtGRy1qVnQtVWM">
</iframe>
