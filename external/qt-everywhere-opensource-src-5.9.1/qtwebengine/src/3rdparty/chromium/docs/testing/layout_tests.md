# Layout Tests

Layout tests are used by Blink to test many components, including but not
limited to layout and rendering. In general, layout tests involve loading pages
in a test renderer (`content_shell`) and comparing the rendered output or
JavaScript output against an expected output file.

[TOC]

## Running Layout Tests

### Initial Setup

Before you can run the layout tests, you need to build the `blink_tests` target
to get `content_shell` and all of the other needed binaries.

```bash
ninja -C out/Release blink_tests
```

On **Android** (layout test support
[currently limited to KitKat and earlier](https://crbug.com/567947)) you need to
build and install `content_shell_apk` instead. See also:
[Android Build Instructions](../android_build_instructions.md).

```bash
ninja -C out/Default content_shell_apk
adb install -r out/Default/apks/ContentShell.apk
```

On **Mac**, you probably want to strip the content_shell binary before starting
the tests. If you don't, you'll have 5-10 running concurrently, all stuck being
examined by the OS crash reporter. This may cause other failures like timeouts
where they normally don't occur.

```bash
strip ./xcodebuild/{Debug,Release}/content_shell.app/Contents/MacOS/content_shell
```

### Running the Tests

TODO: mention `testing/xvfb.py`

The test runner script is in
`third_party/WebKit/Tools/Scripts/run-webkit-tests`.

To specify which build directory to use (e.g. out/Default, out/Release,
out/Debug) you should pass the `-t` or `--target` parameter. For example, to
use the build in `out/Default`, use:

```bash
python third_party/WebKit/Tools/Scripts/run-webkit-tests -t Default
```

For Android (if your build directory is `out/android`):

```bash
python third_party/WebKit/Tools/Scripts/run-webkit-tests -t android --android
```

Tests marked as `[ Skip ]` in
[TestExpectations](../../third_party/WebKit/LayoutTests/TestExpectations)
won't be run at all, generally because they cause some intractable tool error.
To force one of them to be run, either rename that file or specify the skipped
test as the only one on the command line (see below).

Note that currently only the tests listed in
[SmokeTests](../../third_party/WebKit/LayoutTests/SmokeTests)
are run on the Android bots, since running all layout tests takes too long on
Android (and may still have some infrastructure issues). Most developers focus
their Blink testing on Linux. We rely on the fact that the Linux and Android
behavior is nearly identical for scenarios outside those covered by the smoke
tests.

To run only some of the tests, specify their directories or filenames as
arguments to `run_webkit_tests.py` relative to the layout test directory
(`src/third_party/WebKit/LayoutTests`). For example, to run the fast form tests,
use:

```bash
Tools/Scripts/run-webkit-tests fast/forms
```

Or you could use the following shorthand:

```bash
Tools/Scripts/run-webkit-tests fast/fo\*
```

*** promo
Example: To run the layout tests with a debug build of `content_shell`, but only
test the SVG tests and run pixel tests, you would run:

```bash
Tools/Scripts/run-webkit-tests -t Default svg
```
***

As a final quick-but-less-robust alternative, you can also just use the
content_shell executable to run specific tests by using (for Windows):

```bash
out/Default/content_shell.exe --run-layout-test --no-sandbox full_test_source_path
```

as in:

```bash
out/Default/content_shell.exe --run-layout-test --no-sandbox \
    c:/chrome/src/third_party/WebKit/LayoutTests/fast/forms/001.html
```

but this requires a manual diff against expected results, because the shell
doesn't do it for you.

To see a complete list of arguments supported, run: `run-webkit-tests --help`

*** note
**Linux Note:** We try to match the Windows render tree output exactly by
matching font metrics and widget metrics. If there's a difference in the render
tree output, we should see if we can avoid rebaselining by improving our font
metrics. For additional information on Linux Layout Tests, please see
[docs/layout_tests_linux.md](../layout_tests_linux.md).
***

*** note
**Mac Note:** While the tests are running, a bunch of Appearance settings are
overridden for you so the right type of scroll bars, colors, etc. are used.
Your main display's "Color Profile" is also changed to make sure color
correction by ColorSync matches what is expected in the pixel tests. The change
is noticeable, how much depends on the normal level of correction for your
display. The tests do their best to restore your setting when done, but if
you're left in the wrong state, you can manually reset it by going to
System Preferences → Displays → Color and selecting the "right" value.
***

### Test Harness Options

This script has a lot of command line flags. You can pass `--help` to the script
to see a full list of options. A few of the most useful options are below:

| Option                      | Meaning |
|:----------------------------|:--------------------------------------------------|
| `--debug`                   | Run the debug build of the test shell (default is release). Equivalent to `-t Debug` |
| `--nocheck-sys-deps`        | Don't check system dependencies; this allows faster iteration. |
| `--verbose`                 |	Produce more verbose output, including a list of tests that pass. |
| `--no-pixel-tests`          | Disable the pixel-to-pixel PNG comparisons and image checksums for tests that don't call `testRunner.dumpAsText()` |
| `--reset-results`           |	Write all generated results directly into the given directory, overwriting what's there. |
| `--new-baseline`            |	Write all generated results into the most specific platform directory, overwriting what's there. Equivalent to `--reset-results --add-platform-expectations` |
| `--renderer-startup-dialog` | Bring up a modal dialog before running the test, useful for attaching a debugger. |
| `--fully-parallel`          | Run tests in parallel using as many child processes as the system has cores. |
| `--driver-logging`          | Print C++ logs (LOG(WARNING), etc).  |

## Success and Failure

A test succeeds when its output matches the pre-defined expected results. If any
tests fail, the test script will place the actual generated results, along with
a diff of the actual and expected results, into
`src/out/Default/layout_test_results/`, and by default launch a browser with a
summary and link to the results/diffs.

The expected results for tests are in the
`src/third_party/WebKit/LayoutTests/platform` or alongside their respective
tests.

*** note
Tests which use [testharness.js](https://github.com/w3c/testharness.js/)
do not have expected result files if all test cases pass.
***

A test that runs but produces the wrong output is marked as "failed", one that
causes the test shell to crash is marked as "crashed", and one that takes longer
than a certain amount of time to complete is aborted and marked as "timed out".
A row of dots in the script's output indicates one or more tests that passed.

## Test expectations

The
[TestExpectations](../../WebKit/LayoutTests/TestExpectations) file (and related
files, including
[skia_test_expectations.txt](../../skia/skia_test_expectations.txt))
contains the list of all known layout test failures. See
[Test Expectations](./layout_test_expectations.md)
for more on this.

## Testing Runtime Flags

There are two ways to run layout tests with additional command-line arguments:

* Using `--additional-driver-flag`:

  ```bash
  run-webkit-tests --additional-driver-flag=--blocking-repaint
  ```

  This tells the test harness to pass `--blocking-repaint` to the
  content_shell binary.

  It will also look for flag-specific expectations in
  `LayoutTests/FlagExpectations/blocking-repaint`, if this file exists. The
  suppressions in this file override the main TestExpectations file.

* Using a *virtual test suite* defined in
  [LayoutTests/VirtualTestSuites](https://code.google.com/p/chromium/codesearch#chromium/src/third_party/WebKit/LayoutTests/VirtualTestSuites).
  A virtual test suite runs a subset of layout tests under a specific path with
  additional flags. For example, you could test a (hypothetical) new mode for
  repainting using the following virtual test suite:

  ```json
  {
    "prefix": "blocking_repaint",
    "base": "fast/repaint",
    "args": ["--blocking-repaint"],
  }
  ```

  This will create new "virtual" tests of the form
  `virtual/blocking_repaint/fast/repaint/...`` which correspond to the files
  under `LayoutTests/fast/repaint` and pass `--blocking-repaint` to
  content_shell when they are run.

  These virtual tests exist in addition to the original `fast/repaint/...`
  tests. They can have their own expectations in TestExpectations, and their own
  baselines.  The test harness will use the non-virtual baselines as a fallback.
  However, the non-virtual expectations are not inherited: if
  `fast/repaint/foo.html` is marked `[ Fail ]`, the test harness still expects
  `virtual/blocking_repaint/fast/repaint/foo.html` to pass. If you expect the
  virtual test to also fail, it needs its own suppression.

  The "prefix" value does not have to be unique. This is useful if you want to
  run multiple directories with the same flags (but see the notes below about
  performance). Using the same prefix for different sets of flags is not
  recommended.

For flags whose implementation is still in progress, virtual test suites and
flag-specific expectations represent two alternative strategies for testing.
Consider the following when choosing between them:

* The
  [waterfall builders](https://dev.chromium.org/developers/testing/chromium-build-infrastructure/tour-of-the-chromium-buildbot)
  and [try bots](https://dev.chromium.org/developers/testing/try-server-usage)
  will run all virtual test suites in addition to the non-virtual tests.
  Conversely, a flag-specific expectations file won't automatically cause the
  bots to test your flag - if you want bot coverage without virtual test suites,
  you will need to set up a dedicated bot for your flag.

* Due to the above, virtual test suites incur a performance penalty for the
  commit queue and the continuous build infrastructure. This is exacerbated by
  the need to restart `content_shell` whenever flags change, which limits
  parallelism. Therefore, you should avoid adding large numbers of virtual test
  suites. They are well suited to running a subset of tests that are directly
  related to the feature, but they don't scale to flags that make deep
  architectural changes that potentially impact all of the tests.

## Tracking Test Failures

All bugs, associated with layout test failures must have the
[Test-Layout](https://crbug.com/?q=label:Test-Layout) label. Depending on how
much you know about the bug, assign the status accordingly:

* **Unconfirmed** -- You aren't sure if this is a simple rebaseline, possible
  duplicate of an existing bug, or a real failure
* **Untriaged** -- Confirmed but unsure of priority or root cause.
* **Available** -- You know the root cause of the issue.
* **Assigned** or **Started** -- You will fix this issue.

When creating a new layout test bug, please set the following properties:

* Components: a sub-component of Blink
* OS: **All** (or whichever OS the failure is on)
* Priority: 2 (1 if it's a crash)
* Type: **Bug**
* Labels: **Test-Layout**

You can also use the _Layout Test Failure_ template, which will pre-set these
labels for you.

## Writing Layout Tests

###  Pixel Tests

TODO: Write documentation here.

### Reference Tests

TODO: Write documentation here.

### Script Tests

These tests use a JavaScript test harness and test cases written in script to
exercise features and make assertions about the behavior. Generally, new tests
are written using the [testharness.js](https://github.com/w3c/testharness.js/)
test harness, which is also heavily used in the cross-vendor
[web-platform-tests](https://github.com/w3c/web-platform-tests) project. Tests
written with testharness.js generally look something like the following:

```html
<!DOCTYPE html>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
<script>
test(t => {
  var x = true;
  assert_true(x);
}, "Truth is true.");
</script>
```

Many older tests are written using the **js-test**
(`LayoutTests/resources/js-test.js`) test harness. This harness is
**deprecated**, and should not be used for new tests. The tests call
`testRunner.dumpAsText()` to signal that the page content should be dumped and
compared against an \*-expected.txt file, and optionally
`testRunner.waitUntilDone()` and `testRunner.notifyDone()` for asynchronous
tests.

### Tests that use a HTTP Server

By default, tests are loaded as if via `file:` URLs. Some web platform features
require tests served via HTTP or HTTPS, for example relative paths (`src=/foo`)
or features restricted to secure protocols.

HTTP tests are those tests that are under `LayoutTests/http/tests` (or virtual
variants). Use a locally running HTTP server (Apache) to run. Tests are served
off of ports 8000, 8080 for HTTP and 8443 for HTTPS. If you run the tests using
`run-webkit-tests`, the server will be started automatically.To run the server
manually to reproduce or debug a failure:

```bash
cd src/third_party/WebKit/Tools/Scripts
run-blink-httpd start
```

The layout tests will be served from `http://127.0.0.1:8000`. For example, to
run the test `http/tests/serviceworker/chromium/service-worker-allowed.html`,
navigate to
`http://127.0.0.1:8000/serviceworker/chromium/service-worker-allowed.html`. Some
tests will behave differently if you go to 127.0.0.1 instead of localhost, so
use 127.0.0.1.

To kill the server, run `run-blink-httpd --server stop`, or just use `taskkill`
or the Task Manager on Windows, and `killall` or Activity Monitor on MacOS.

The test server sets up an alias to `LayoutTests/resources` directory. In HTTP
tests, you can access the testing framework at e.g.
`src="/js-test-resources/js-test.js"`.

### Writing tests that need to paint, raster, or draw a frame of intermediate output

A layout test does not actually draw frames of output until the test exits. If
it is required to generate a painted frame, then use
`window.testRunner.displayAsyncThen`, which will run the machinery to put up a
frame, then call the passed callback. There is also a library at
`fast/repaint/resources/text-based-repaint.js` to help with writing paint
invalidation and repaint tests.

#### Layout test support for `testRunner`

Some layout tests rely on the testRunner object to expose configuration for
mocking the platform. This is provided in content_shell, here's a UML diagram of
testRunner bindings configuring platform implementation:

[![UML of testRunner bindings configuring platform implementation](https://docs.google.com/drawings/u/1/d/1KNRNjlxK0Q3Tp8rKxuuM5mpWf4OJQZmvm9_kpwu_Wwg/export/svg?id=1KNRNjlxK0Q3Tp8rKxuuM5mpWf4OJQZmvm9_kpwu_Wwg&pageid=p)](https://docs.google.com/drawings/d/1KNRNjlxK0Q3Tp8rKxuuM5mpWf4OJQZmvm9_kpwu_Wwg/edit)

[Writing reliable layout tests](https://docs.google.com/document/d/1Yl4SnTLBWmY1O99_BTtQvuoffP8YM9HZx2YPkEsaduQ/edit)

## Debugging Layout Tests

After the layout tests run, you should get a summary of tests that pass or fail.
If something fails unexpectedly (a new regression), you will get a content_shell
window with a summary of the unexpected failures. Or you might have a failing
test in mind to investigate. In any case, here are some steps and tips for
finding the problem.

* Take a look at the result. Sometimes tests just need to be rebaselined (see
  below) to account for changes introduced in your patch.
    * Load the test into a trunk Chrome or content_shell build and look at its
      result. (For tests in the http/ directory, start the http server first.
      See above. Navigate to `http://localhost:8000/` and proceed from there.)
      The best tests describe what they're looking for, but not all do, and
      sometimes things they're not explicitly testing are still broken. Compare
      it to Safari, Firefox, and IE if necessary to see if it's correct. If
      you're still not sure, find the person who knows the most about it and
      ask.
    * Some tests only work properly in content_shell, not Chrome, because they
      rely on extra APIs exposed there.
    * Some tests only work properly when they're run in the layout-test
      framework, not when they're loaded into content_shell directly. The test
      should mention that in its visible text, but not all do. So try that too.
      See "Running the tests", above.
* If you think the test is correct, confirm your suspicion by looking at the
  diffs between the expected result and the actual one.
    * Make sure that the diffs reported aren't important. Small differences in
      spacing or box sizes are often unimportant, especially around fonts and
      form controls. Differences in wording of JS error messages are also
      usually acceptable.
    * `./run_webkit_tests.py path/to/your/test.html --full-results-html` will
      produce a page including links to the expected result, actual result, and
      diff.
    * Add the `--sources` option to `run_webkit_tests.py` to see exactly which
      expected result it's comparing to (a file next to the test, something in
      platform/mac/, something in platform/chromium-win/, etc.)
    * If you're still sure it's correct, rebaseline the test (see below).
      Otherwise...
* If you're lucky, your test is one that runs properly when you navigate to it
  in content_shell normally. In that case, build the Debug content_shell
  project, fire it up in your favorite debugger, and load the test file either
  from a file:// URL.
    * You'll probably be starting and stopping the content_shell a lot. In VS,
      to save navigating to the test every time, you can set the URL to your
      test (file: or http:) as the command argument in the Debugging section of
      the content_shell project Properties.
    * If your test contains a JS call, DOM manipulation, or other distinctive
      piece of code that you think is failing, search for that in the Chrome
      solution. That's a good place to put a starting breakpoint to start
      tracking down the issue.
    * Otherwise, you're running in a standard message loop just like in Chrome.
      If you have no other information, set a breakpoint on page load.
* If your test only works in full layout-test mode, or if you find it simpler to
  debug without all the overhead of an interactive session, start the
  content_shell with the command-line flag `--run-layout-test`, followed by the
  URL (file: or http:) to your test. More information about running layout tests
  in content_shell can be found [here](./layout_tests_in_content_shell.md).
    * In VS, you can do this in the Debugging section of the content_shell
      project Properties.
    * Now you're running with exactly the same API, theme, and other setup that
      the layout tests use.
    * Again, if your test contains a JS call, DOM manipulation, or other
      distinctive piece of code that you think is failing, search for that in
      the Chrome solution. That's a good place to put a starting breakpoint to
      start tracking down the issue.
    * If you can't find any better place to set a breakpoint, start at the
      `TestShell::RunFileTest()` call in `content_shell_main.cc`, or at
      `shell->LoadURL() within RunFileTest()` in `content_shell_win.cc`.
* Debug as usual. Once you've gotten this far, the failing layout test is just a
  (hopefully) reduced test case that exposes a problem.

### Debugging HTTP Tests

To run the server manually to reproduce/debug a failure:

```bash
cd src/third_party/WebKit/Tools/Scripts
run-blink-httpd start
```

The layout tests will be served from `http://127.0.0.1:8000`. For example, to
run the test
`LayoutTest/http/tests/serviceworker/chromium/service-worker-allowed.html`,
navigate to
`http://127.0.0.1:8000/serviceworker/chromium/service-worker-allowed.html`. Some
tests will behave differently if you go to 127.0.0.1 vs localhost, so use
127.0.0.1.

To kill the server, run `run-blink-httpd --server stop`, or just use `taskkill`
or the Task Manager on Windows, and `killall` or Activity Monitor on MacOS.

The test server sets up an alias to `LayoutTests/resources` directory. In HTTP
tests, you can access testing framework at e.g.
`src="/js-test-resources/js-test.js"`.

### Tips

Check https://test-results.appspot.com/ to see how a test did in the most recent
~100 builds on each builder (as long as the page is being updated regularly).

A timeout will often also be a text mismatch, since the wrapper script kills the
content_shell before it has a chance to finish. The exception is if the test
finishes loading properly, but somehow hangs before it outputs the bit of text
that tells the wrapper it's done.

Why might a test fail (or crash, or timeout) on buildbot, but pass on your local
machine?
* If the test finishes locally but is slow, more than 10 seconds or so, that
  would be why it's called a timeout on the bot.
* Otherwise, try running it as part of a set of tests; it's possible that a test
  one or two (or ten) before this one is corrupting something that makes this
  one fail.
* If it consistently works locally, make sure your environment looks like the
  one on the bot (look at the top of the stdio for the webkit_tests step to see
  all the environment variables and so on).
* If none of that helps, and you have access to the bot itself, you may have to
  log in there and see if you can reproduce the problem manually.

### Debugging Inspector Tests

* Add `window.debugTest = true;` to your test code as follows:

  ```javascript
  window.debugTest = true;
  function test() {
    /* TEST CODE */
  }
  ```

* Do one of the following:
    * Option A) Run from the chromium/src folder:
      `blink/tools/run_layout_tests.sh
      --additional_driver_flag='--remote-debugging-port=9222'
      --time-out-ms=6000000`
    * Option B) If you need to debug an http/tests/inspector test, start httpd
      as described above. Then, run content_shell:
      `out/Default/content_shell --remote-debugging-port=9222 --run-layout-test
      http://127.0.0.1:8000/path/to/test.html`
* Open `http://localhost:9222` in a stable/beta/canary Chrome, click the single
  link to open the devtools with the test loaded.
* You may need to replace devtools.html with inspector.html in your URL (or you
  can use local chrome inspection of content_shell from chrome://inspect
  instead)
* In the loaded devtools, set any required breakpoints and execute `test()` in
  the console to actually start the test.

## Rebaselining Layout Tests

*** promo
To automatically re-baseline tests across all Chromium platforms, using the
buildbot results, see the
[Rebaselining keywords in TestExpectations](./layout_test_expectations.md)
and the
[Rebaselining Tool](https://trac.webkit.org/wiki/Rebaseline).
Alternatively, to manually run and test and rebaseline it on your workstation,
read on.
***

By default, text-only tests (ones that call `testRunner.dumpAsText()`) produce
only text results. Other tests produce both new text results and new image
results (the image baseline comprises two files, `-expected.png` and
  `-expected.checksum`). So you'll need either one or three `-expected.\*` files
in your new baseline, depending on whether you have a text-only test or not. If
you enable `--no-pixel-tests`, only new text results will be produced, even for
tests that do image comparisons.

```bash
cd src/third_party/WebKit
Tools/Scripts/run-webkit-tests --new-baseline foo/bar/test.html
```

The above command will generate a new baseline for
`LayoutTests/foo/bar/test.html` and put the output files in the right place,
e.g.
`LayoutTests/platform/chromium-win/LayoutTests/foo/bar/test-expected.{txt,png,checksum}`.

When you rebaseline a test, make sure your commit description explains why the
test is being re-baselined. If this is a special case (i.e., something we've
decided to be different with upstream), please put a README file next to the new
expected output explaining the difference.

## W3C Tests

In addition to layout tests developed and run just by the Blink team, there are
also W3C conformance tests. For more info, see
[Importing the W3C Tests](https://www.chromium.org/blink/importing-the-w3c-tests).

## Known Issues

See
[bugs with the component Blink>Infra](https://bugs.chromium.org/p/chromium/issues/list?can=2&q=component%3ABlink%3EInfra)
for issues related to Blink tools, include the layout test runner.

* Windows and Linux: Do not copy and paste while the layout tests are running,
  as it may interfere with the editing/pasteboard and other clipboard-related
  tests. (Mac tests swizzle NSClipboard to avoid any conflicts).
* If QuickTime is not installed, the plugin tests
  `fast/dom/object-embed-plugin-scripting.html` and
  `plugins/embed-attributes-setting.html` are expected to fail.
