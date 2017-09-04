# Android Build Instructions

[TOC]

## Prerequisites

A Linux build machine capable of building [Chrome for
Linux](https://chromium.googlesource.com/chromium/src/+/master/docs/linux_build_instructions_prerequisites.md).
Other (Mac/Windows) platforms are not supported for Android.

## Getting the code

First, check out and install the [depot\_tools
package](https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html#_setting_up).

Then, if you have no existing checkout, create your source directory and
get the code:

```shell
mkdir ~/chromium && cd ~/chromium
fetch --nohooks android # This will take 30 minutes on a fast connection
```

If you have an existing Linux checkout, you can add Android support by
appending `target_os = ['android']` to your .gclient file (in the
directory above src):

```shell
cat > .gclient <<EOF
 solutions = [ ...existing stuff in here... ]
 target_os = [ 'android' ] # Add this to get Android stuff checked out.
EOF
```

Then run gclient sync to get the Android stuff checked out:

```shell
gclient sync
```

## (Optional) Check out LKGR

If you want a single build of Chromium in a known good state, sync to
the LKGR ("last known good revision"). You can find it
[here](http://chromium-status.appspot.com/lkgr), and the last 100
[here](http://chromium-status.appspot.com/revisions). Run:

```shell
gclient sync --nohooks -r <lkgr-sha1>
```

This is not needed for a typical developer workflow; only for one-time
builds of Chromium.

## Configure GN

Create a build directory and set the build flags with:

```shell
gn args out/Default
```

 You can replace out/Default with another name you choose inside the out
directory.

Also be aware that some scripts (e.g. tombstones.py, adb_gdb.py)
require you to set `CHROMIUM_OUTPUT_DIR=out/Default`.

This command will bring up your editor with the GN build args. In this
file add:

```
target_os = "android"
target_cpu = "arm"  # (default)
is_debug = true  # (default)

# Other args you may want to set:
is_component_build = true
is_clang = true
symbol_level = 1  # Faster build with fewer symbols. -g1 rather than -g2
enable_incremental_javac = true  # Much faster; experimental
```

You can also specify `target_cpu` values of "x86" and "mipsel". Re-run
gn args on that directory to edit the flags in the future. See the [GN
build
configuration](https://www.chromium.org/developers/gn-build-configuration)
page for other flags you may want to set.

### Install build dependencies

Update the system packages required to build by running:

```shell
./build/install-build-deps-android.sh
```

Make also sure that OpenJDK 1.7 is selected as default:

`sudo update-alternatives --config javac`
`sudo update-alternatives --config java`
`sudo update-alternatives --config javaws`
`sudo update-alternatives --config javap`
`sudo update-alternatives --config jar`
`sudo update-alternatives --config jarsigner`

### Synchronize sub-directories.

```shell
gclient sync
```

## Build and install the APKs

If the `adb_install_apk.py` script below fails, make sure aapt is in
your PATH. If not, add aapt's path to your PATH environment variable (it
should be
`/path/to/src/third_party/android_tools/sdk/build-tools/{latest_version}/`).

Prepare the environment:

```shell
. build/android/envsetup.sh
```

### Plug in your Android device

Make sure your Android device is plugged in via USB, and USB Debugging
is enabled.

To enable USB Debugging:

*   Navigate to Settings \> About Phone \> Build number
*   Click 'Build number' 7 times
*   Now navigate back to Settings \> Developer Options
*   Enable 'USB Debugging' and follow the prompts

You may also be prompted to allow access to your PC once your device is
plugged in.

You can check if the device is connected by running:

```shell
third_party/android_tools/sdk/platform-tools/adb devices
```

Which prints a list of connected devices. If not connected, try
unplugging and reattaching your device.

### Build the full browser

```shell
ninja -C out/Release chrome_public_apk
```

And deploy it to your Android device:

```shell
CHROMIUM_OUTPUT_DIR=$gndir build/android/adb_install_apk.py $gndir/apks/ChromePublic.apk # for gn.
```

The app will appear on the device as "Chromium".

### Build Content shell

Wraps the content module (but not the /chrome embedder). See
[http://www.chromium.org/developers/content-module](http://www.chromium.org/developers/content-module)
for details on the content module and content shell.

```shell
ninja -C out/Release content_shell_apk
build/android/adb_install_apk.py out/Release/apks/ContentShell.apk
```

this will build and install an Android apk under
`out/Release/apks/ContentShell.apk`. (Where `Release` is the name of your build
directory.)

If you use custom out dir instead of standard out/ dir, use
CHROMIUM_OUT_DIR env.

```shell
export CHROMIUM_OUT_DIR=out_android
```

### Build WebView shell

[Android WebView](http://developer.android.com/reference/android/webkit/WebView.html)
is a system framework component. Since Android KitKat, it is implemented using
Chromium code (based off the [content module](http://dev.chromium.org/developers/content-module)).
It is possible to test modifications to WebView using a simple test shell. The
WebView shell is a view with a URL bar at the top (see [code](https://code.google.com/p/chromium/codesearch#chromium/src/android_webview/test/shell/src/org/chromium/android_webview/test/AwTestContainerView.java))
and is **independent** of the WebView **implementation in the Android system** (
the WebView shell is essentially a standalone unbundled app).
As drawback, the shell runs in non-production rendering mode only.

```shell
ninja -C out/Release android_webview_apk
build/android/adb_install_apk.py out/Release/apks/AndroidWebView.apk
```

If, instead, you want to build the complete Android WebView framework component and test the effect of your chromium changes in other Android app using the WebView, you should follow the [Android AOSP + chromium WebView instructions](http://www.chromium.org/developers/how-tos/build-instructions-android-webview)

### Running

Set [command line flags](https://www.chromium.org/developers/how-tos/run-chromium-with-flags) if necessary.

For Content shell:

```shell
build/android/adb_run_content_shell  http://example.com
```

For Chrome public:

```shell
build/android/adb_run_chrome_public  http://example.com
```

For Android WebView shell:

```shell
build/android/adb_run_android_webview_shell http://example.com
```

### Logging and debugging

Logging is often the easiest way to understand code flow. In C++ you can print
log statements using the LOG macro or printf(). In Java, you can print log
statements using [android.util.Log](http://developer.android.com/reference/android/util/Log.html):

`Log.d("sometag", "Reticulating splines progress = " + progress);`

You can see these log statements using adb logcat:

```shell
adb logcat...01-14 11:08:53.373 22693 23070 D sometag: Reticulating splines progress = 0.99
```

You can debug Java or C++ code. To debug C++ code, use one of the
following commands:

```shell
build/android/adb_gdb_content_shell
build/android/adb_gdb_chrome_public
build/android/adb_gdb_android_webview_shell http://example.com
```

See [Debugging Chromium on Android](https://www.chromium.org/developers/how-tos/debugging-on-android)
for more on debugging, including how to debug Java code.

### Testing

For information on running tests, see [android\_test\_instructions.md](https://chromium.googlesource.com/chromium/src/+/master/docs/android_test_instructions.md).

### Faster Edit/Deploy (GN only)

GN's "incremental install" uses reflection and side-loading to speed up the edit
& deploy cycle (normally < 10 seconds).

*   Make sure to set` is_component_build = true `in your GN args
*   All apk targets have \*`_incremental` targets defined (e.g.
    `chrome_public_apk_incremental`)

Here's an example:

```shell
ninja -C out/Default chrome_public_apk_incremental
out/Default/bin/install_chrome_public_apk_incremental -v
```

For gunit tests (note that run_*_incremental automatically add
--fast-local-dev when calling test\_runner.py):

```shell
ninja -C out/Default base_unittests_incremental
out/Default/bin/run_base_unittests_incremental
```

For instrumentation tests:

```shell
ninja -C out/Default chrome_public_test_apk_incremental
out/Default/bin/run_chrome_public_test_apk_incremental
```

To uninstall:

```shell
out/Default/bin/install_chrome_public_apk_incremental -v --uninstall
```

### Miscellaneous

#### Rebuilding libchrome.so for a particular release

These instructions are only necessary for Chrome 51 and earlier.

In the case where you want to modify the native code for an existing
release of Chrome for Android (v25+) you can do the following steps.
Note that in order to get your changes into the official release, you'll
need to send your change for a codereview using the regular process for
committing code to chromium.

1.  Open Chrome on your Android device and visit chrome://version
2.  Copy down the id listed next to "Build ID:"
3.  Go to
    [http://storage.googleapis.com/chrome-browser-components/BUILD\_ID\_FROM\_STEP\_2/index.html](http://storage.googleapis.com/chrome-browser-components/BUILD_ID_FROM_STEP_2/index.html)
4.  Download the listed files and follow the steps in the README.
