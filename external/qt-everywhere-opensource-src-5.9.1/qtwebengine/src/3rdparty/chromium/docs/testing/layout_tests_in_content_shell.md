# Running layout tests using the content shell

## Basic usage

Layout tests can be run with `content_shell`. To just dump the render tree, use
the `--run-layout-test` flag:

```bash
out/Default/content_shell --run-layout-test foo.html
```

### Compiling

If you want to run layout tests,
[build the target `blink_tests`](layout_tests.md); this includes all the other
binaries required to run the tests.

### Running

You can run layout tests using `run-webkit-tests` (in
`src/third_party/WebKit/Tools/Scripts`).

```bash
third_party/WebKit/Tools/Scripts/run-webkit-tests storage/indexeddb
```

or execute the shell directly:

```bash
out/Default/content_shell --remote-debugging-port=9222
```

This allows you see how your changes look in Chromium, and even connect with
devtools (by going to http://127.0.0.1:9222 from another window) to inspect your
freshly compiled Blink.

*** note
On the Mac, use `Content Shell.app`, not `content_shell`.

```bash
out/Default/Content\ Shell.app/Contents/MacOS/Content\ Shell --remote-debugging-port=9222
```
***

### Debugging Renderer Crashes

To debug a renderer crash, ask Content Shell to wait for you to attach a
debugger once it spawns a renderer process by adding the
`--renderer-startup-dialog` flag:

```bash
out/Default/content_shell --renderer-startup-dialog
```

Debugging workers and other subprocesses is simpler with
`--wait-for-debugger-children`, which can have one of two values: `plugin` or
`renderer`.

## Future Work

### Reusing existing testing objects

To avoid writing (and maintaining!) yet another test controller, it is desirable
to reuse an existing test controller. A possible solution would be to change
DRT's test controller to not depend on DRT's implementation of the Blink
objects, but rather on the Blink interfaces. In addition, we would need to
extract an interface from the test shell object that can be implemented by
content shell. This would allow for directly using DRT's test controller in
content shell.
