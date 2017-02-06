# Heap Profiling with MemoryInfra

As of Chrome 48, MemoryInfra supports heap profiling. The core principle is
a solution that JustWorks™ on all platforms without patching or rebuilding,
intergrated with the chrome://tracing ecosystem.

[TOC]

## How to Use

 1. Start Chrome with the `--enable-heap-profiling` switch. This will make
    Chrome keep track of all allocations.

 2. Grab a [MemoryInfra][memory-infra] trace. For best results, start tracing
    first, and _then_ open a new tab that you want to trace. Furthermore,
    enabling more categories (besides memory-infra) will yield more detailed
    information in the heap profiler backtraces.

 3. When the trace has been collected, select a heavy memory dump indicated by
    a purple ![M][m-purple] dot. Heap dumps are only included in heavy memory
    dumps.

 4. In the analysis view, cells marked with a triple bar icon (☰) contain heap
    dumps. Select such a cell.

      ![Cells containing a heap dump][cells-heap-dump]

 5. Scroll down all the way to _Heap Details_.

 6. Pinpoint the memory bug and live happily ever after.

[memory-infra]:    memory_infra.md
[m-purple]:        https://storage.googleapis.com/chromium-docs.appspot.com/d7bdf4d16204c293688be2e5a0bcb2bf463dbbc3
[cells-heap-dump]: https://storage.googleapis.com/chromium-docs.appspot.com/a24d80d6a08da088e2e9c8b2b64daa215be4dacb

### Native stack traces

By default heap profiling collects pseudo allocation traces, which are based
on trace events. I.e. frames in allocation traces correspond to trace events
that were active at the time of allocations, and are not real function names.
However, you can build a special Linux / Android build that will collect
real C/C++ stack traces.

 1. Build with the following GN flags:

	Linux

        enable_profiling = true


	Android

        arm_use_thumb = false
        enable_profiling = true

 2. Start Chrome with `--enable-heap-profiling=native` switch (notice
 	`=native` part).

 	On Android use the command line tool before starting the app:

        build/android/adb_chrome_public_command_line --enable-heap-profiling=native

 	(run the tool with an empty argument `''` to clear the command line)

 3. Grab a [MemoryInfra][memory-infra] trace. You don't need any other
 	categories besides `memory-infra`.

 4. Save the grabbed trace file. This step is needed because freshly
 	taken trace file contains raw addresses (which look like `pc:dcf5dbf8`)
 	instead of function names, and needs to be symbolized.

 4. Symbolize the trace file. During symbolization addresses are resolved to
 	the corresponding function names and trace file is rewritten (but a backup
 	is saved with `.BACKUP` extension).

	Linux

        third_party/catapult/tracing/bin/symbolize_trace <trace file>

	Android

        third_party/catapult/tracing/bin/symbolize_trace --output-directory out/Release <trace file>

	(note `--output-directory` and make sure it's right for your setup)

 5. Load the trace file in `chrome://tracing`. Locate a purple ![M][m-purple]
 	dot, and continue from step *3* from the instructions above. Native stack
 	traces will be shown in the _Heap Details_ pane.

## Heap Details

The heap details view contains a tree that represents the heap. The size of the
root node corresponds to the selected allocator cell.

*** aside
The size value in the heap details view will not match the value in the selected
analysis view cell exactly. There are three reasons for this. First, the heap
profiler reports the memory that _the program requested_, whereas the allocator
reports the memory that it _actually allocated_ plus its own bookkeeping
overhead. Second, allocations that happen early --- before Chrome knows that
heap profiling is enabled --- are not captured by the heap profiler, but they
are reported by the allocator. Third, tracing overhead is not discounted by the
heap profiler.
***

The heap can be broken down in two ways: by _backtrace_ (marked with an ƒ), and
by _type_ (marked with a Ⓣ). When tracing is enabled, Chrome records trace
events, most of which appear in the flame chart in timeline view. At every
point in time these trace events form a pseudo stack, and a vertical slice
through the flame chart is like a backtrace. This corresponds to the ƒ nodes in
the heap details view.  Hence enabling more tracing categories will give a more
detailed breakdown of the heap.

The other way to break down the heap is by object type. At the moment this is
only supported for PartitionAlloc.

*** aside
In official builds, only the most common type names are included due to binary
size concerns. Development builds have full type information.
***

To keep the trace log small, uninteresting information is omitted from heap
dumps. The long tail of small nodes is not dumped, but grouped in an `<other>`
node instead. Note that altough these small nodes are insignificant on their
own, together they can be responsible for a significant portion of the heap. The
`<other>` node is large in that case.

## Example

In the trace below, `ParseAuthorStyleSheet` is called at some point.

![ParseAuthorStyleSheet pseudo stack][pseudo-stack]

The pseudo stack of trace events corresponds to the tree of ƒ nodes below. Of
the 23.5 MiB of memory allocated with PartitionAlloc, 1.9 MiB was allocated
inside `ParseAuthorStyleSheet`, either directly, or at a deeper level (like
`CSSParserImpl::parseStyleSheet`).

![Memory Allocated in ParseAuthorStyleSheet][break-down-by-backtrace]

By expanding `ParseAuthorStyleSheet`, we can see which types were allocated
there. Of the 1.9 MiB, 371 KiB was spent on `ImmutableStylePropertySet`s, and
238 KiB was spent on `StringImpl`s.

![ParseAuthorStyleSheet broken down by type][break-down-by-type]

It is also possible to break down by type first, and then by backtrace. Below
we see that of the 23.5 MiB allocated with PartitionAlloc, 1 MiB is spent on
`Node`s, and about half of the memory spent on nodes was allocated in
`HTMLDocumentParser`.

![The PartitionAlloc heap broken down by type first and then by backtrace][type-then-backtrace]

Heap dump diffs are fully supported by trace viewer. Select a heavy memory dump
(a purple dot), then with the control key select a heavy memory dump earlier in
time. Below is a diff of theverge.com before and in the middle of loading ads.
We can see that 4 MiB were allocated when parsing the documents in all those
iframes, almost a megabyte of which was due to JavaScript. (Note that this is
memory allocated by PartitionAlloc alone, the total renderer memory increase was
around 72 MiB.)

![Diff of The Verge before and after loading ads][diff]

[pseudo-stack]:            https://storage.googleapis.com/chromium-docs.appspot.com/058e50350836f55724e100d4dbbddf4b9803f550
[break-down-by-backtrace]: https://storage.googleapis.com/chromium-docs.appspot.com/ec61c5f15705f5bcf3ca83a155ed647a0538bbe1
[break-down-by-type]:      https://storage.googleapis.com/chromium-docs.appspot.com/2236e61021922c0813908c6745136953fa20a37b
[type-then-backtrace]:     https://storage.googleapis.com/chromium-docs.appspot.com/c5367dde11476bdbf2d5a1c51674148915573d11
[diff]:                    https://storage.googleapis.com/chromium-docs.appspot.com/802141906869cd533bb613da5f91bd0b071ceb24
