# Reproducing ClusterFuzz bugs locally

ClusterFuzz will report bugs in the bug tracker in the following form:

```
Detailed report: https://cluster-fuzz.appspot.com/testcase?key=...

Fuzzer: libfuzzer_media_pipeline_integration_fuzzer
Job Type: libfuzzer_chrome_asan
Platform Id: linux

Crash Type: Heap-buffer-overflow READ {*}
Crash Address: 0x60500000c64d
Crash State:
  stack_frame1
  stack_frame2
  stack_frame3

Recommended Security Severity: Medium

Regressed: <LINK>

Minimized Testcase (6.86 Kb): <LINK>

Filer: ...
```

You can click the "Detailed report" link for the full stack trace, and
additional information/links.

## Steps to reproduce

1. Download the testcase given by the "Minimized Testcase" link.

2. (**Important**) In the following sections, `$FUZZER_NAME` will be the the
   string specified after the "Fuzzer :" in the report, but *without* the
   "libfuzzer_" prefix. In this case, the `$FUZZER_NAME` is
   "media_pipeline_integration_fuzzer".

3. Follow the steps in one of the subsequent sections (from a chromium
   checkout).  The string specified after the "Job Type: " will be either
   `libfuzzer_chrome_asan`, `libfuzzer_chrome_msan`, or
   `libfuzzer_chrome_ubsan`, indicating which one to use.

### Reproducing ASan bugs

```bash
$ gn gen out/libfuzzer '--args=use_libfuzzer=true is_asan=true enable_nacl=false proprietary_codecs=true'
$ ninja -C out/libfuzzer $FUZZER_NAME
$ out/libfuzzer/$FUZZER_NAME /path/to/repro
```

### Reproducing MSan bugs

```bash
# The gclient sync is necessary to pull in instrumented libraries.
$ GYP_DEFINES='use_prebuilt_instrumented_libraries=1' gclient sync
$ gn gen out/libfuzzer '--args=use_libfuzzer=true is_msan=true msan_track_origins=2 use_prebuilt_instrumented_libraries=true enable_nacl=false proprietary_codecs=true'
$ ninja -C out/libfuzzer $FUZZER_NAME
$ out/libfuzzer/$FUZZER_NAME /path/to/repro
```

### Reproducing UBSan bugs

```bash
$ gn gen out/libfuzzer '--args=use_libfuzzer=true is_ubsan_security=true enable_nacl=false proprietary_codecs=true'
$ ninja -C out/libfuzzer $FUZZER_NAME
$ export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
$ out/libfuzzer/$FUZZER_NAME /path/to/repro
```

*Note*: ClusterFuzz uses release builds by default, so it may be worth adding
"is_debug=false" to your GN args if you are having trouble reproducing a
particular report.
