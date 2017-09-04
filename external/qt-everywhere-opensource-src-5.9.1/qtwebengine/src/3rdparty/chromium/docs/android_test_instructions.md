# Android Test Instructions

[TOC]

## Device Setup

### Physical Device Setup

#### ADB Debugging

In order to allow the ADB to connect to the device, you must enable USB
debugging:

*   Before Android 4.1 (Jelly Bean):
    *   Go to "System Settings"
    *   Go to "Developer options"
    *   Check "USB debugging".
    *   Un-check "Verify apps over USB".
*   On Jelly Bean and above, developer options are hidden by default. To unhide them:
    *   Go to "About phone"
    *   Tap 10 times on "Build number"
    *   The "Developer options" menu will now be available.
    *   Check "USB debugging".
    *   Un-check "Verify apps over USB".

#### Screen

You MUST ensure that the screen stays on while testing: `adb shell svc power
stayon usb` Or do this manually on the device: Settings -> Developer options
-> Stay Awake.

If this option is greyed out, stay awake is probably disabled by policy. In that
case, get another device or log in with a normal, unmanaged account (because the
tests will break in exciting ways if stay awake is off).

#### Enable Asserts

    adb shell setprop debug.assert 1

#### Disable Verify Apps

You may see a dialog like
[this one](http://www.samsungmobileusa.com/simulators/ATT_GalaxyMega/mobile/screens/06-02_12.jpg),
which states, _Google may regularly check installed apps for potentially harmful
behavior._ This can interfere with the test runner. To disable this dialog, run:
`adb shell settings put global package_verifier_enable 0`

### Emulator Setup

#### Option 1

Use an emulator (i.e. Android Virtual Device, AVD): Enabling Intel's
Virtualizaton support provides the fastest, most reliable emulator configuration
available (i.e. x86 emulator with GPU acceleration and KVM support).
Remember to build with `target_arch=ia32` for x86. Otherwise installing the APKs
will fail with `INSTALL_FAILED_NO_MATCHING_ABIS`.

1.  Enable Intel Virtualization support in the BIOS.

2.  Set up your environment:

    ```shell
    . build/android/envsetup.sh
    ```

3.  Install emulator deps:

    ```shell
    build/android/install_emulator_deps.py --api-level=23
    ```

    This script will download Android SDK and place it a directory called
    android\_tools in the same parent directory as your chromium checkout. It
    will also download the system-images for the emulators (i.e. arm and x86).
    Note that this is a different SDK download than the Android SDK in the
    chromium source checkout (i.e. `src/third_party/android_emulator_sdk`).

4.  Run the avd.py script. To start up _num_ emulators use -n. For non-x86 use
    --abi.

    ```shell
    build/android/avd.py --api-level=23
    ```

    This script will attempt to use GPU emulation, so you must be running the
    emulators in an environment with hardware rendering available. See
    `avd.py --help` for more details.

#### Option 2

Alternatively, you can create and run your own emulator using the tools provided
by the Android SDK. When doing so, be sure to enable GPU emulation in hardware
settings, since Chromium requires it to render.

## Building Tests

If you're adding a new test file, you'll need to explicitly add it to a gn
target. If you're adding a test to an existing file, you won't to make gn
changes, but you may be interested in where your test winds up. In either case,
here are some guidelines for where a test belongs:

### C++

C++ test files typically belong in `<top-level directory>_unittests` (e.g.
`base_unittests` for `//base`). There are a few exceptions -- browser tests are
typically their own target (e.g. `content_browsertests` for `//content`, or
`browser_tests` for `//chrome`), and some unit test suites are broken at the
second directory rather than the top-level one.

### Java

Java test files vary a bit more widely than their C++ counterparts:

   - Instrumentation test files -- i.e., tests that will run on a device --
typically belong in either `<top-level directory>_javatests` or
`<top-level directory>_test_java`. Regardless, they'll wind up getting packaged
into one of a few test APKs:
     - `android_webview_test_apk` for anything in `//android_webview`
     - `content_shell_test_apk` for anything in `//content` or below
     - `chrome_public_test_apk` for most things in `//chrome`
     - `chrome_sync_shell_test_apk` in a few exceptional cases
   - JUnit or Robolectric test files -- i.e., tests that will run on the host --
typically belong in `<top-level directory>_junit_tests` (e.g. `base_junit_tests`
for `//base`), though here again there are cases (particularly in
`//components`) where suites are split at the second directory rather than the
top-level one.

Once you know what to build, just do it like you normally would build anything
else, e.g.: `ninja -C out/Release chrome_public_test_apk`

## Running Tests

All functional tests should be runnable via the wrapper scripts generated at
build time:

```sh
<output directory>/bin/run_<target_name> [options]
```

Note that tests are sharded across all attached devices unless explicitly told
to do otherwise by `-d/--device`.

The commands used by the buildbots are printed in the logs. Look at
http://build.chromium.org/ to duplicate the same test command as a particular
builder.

### INSTALL\_FAILED\_CONTAINER\_ERROR or INSTALL\_FAILED\_INSUFFICIENT\_STORAGE

If you see this error when the test runner is attempting to deploy the test
binaries to the AVD emulator, you may need to resize your userdata partition
with the following commands:

```shell
# Resize userdata partition to be 1G
resize2fs android_emulator_sdk/sdk/system-images/android-24/x86/userdata.img 1G

# Set filesystem parameter to continue on errors; Android doesn't like some
# things e2fsprogs does.
tune2fs -e continue android_emulator_sdk/sdk/system-images/android-24/x86/userdata.img
```

## Symbolizing Crashes

Crash stacks are logged and can be viewed using adb logcat. To symbolize the
traces, define `CHROMIUM_OUTPUT_DIR=$OUTDIR` where `$OUTDIR` is the argument you
pass to `ninja -C`, and pipe the output through
`third_party/android_platform/development/scripts/stack`. If
`$CHROMIUM_OUTPUT_DIR` is unset, the script will search `out/Debug` and
`out/Release`. For example:

```shell
# If you build with
ninja -C out/Debug chrome_public_test_apk
# You can run:
adb logcat -d | third_party/android_platform/development/scripts/stack

# If you build with
ninja -C out/android chrome_public_test_apk
# You can run:
adb logcat -d | CHROMIUM_OUTPUT_DIR=out/android third_party/android_platform/development/scripts/stack
# or
export CHROMIUM_OUTPUT_DIR=out/android
adb logcat -d | third_party/android_platform/development/scripts/stack
```

## JUnit tests

JUnit tests are Java unittests running on the host instead of the target device.
They are faster to run and therefore are recommended over instrumentation tests
when possible.

The JUnits tests are usually following the pattern of *target*\_junit\_tests,
for example, `content_junit_tests` and `chrome_junit_tests`.

When adding a new JUnit test, the associated `BUILD.gn` file must be updated.
For example, adding a test to `chrome_junit_tests` requires to update
`chrome/android/BUILD.gn`. If you are a GYP user, you will not need to do that
step in order to run the test locally but it is still required for GN users to
run the test.

```shell
# Build the test suite.
ninja -C out/Release chrome_junit_tests

# Run the test suite.
build/android/test_runner.py junit -s chrome_junit_tests --release -vvv

# Run a subset of tests. You might need to pass the package name for some tests.
build/android/test_runner.py junit -s chrome_junit_tests --release -vvv
-f "org.chromium.chrome.browser.media.*"
```

## Gtests

```shell
# Build a test suite
ninja -C out/Release content_unittests

# Run a test suite
out/Release/bin/run_content_unittests [-vv]

# Run a subset of tests
out/Release/bin/run_content_unittests [-vv] --gtest-filter ByteStreamTest.*
```

## Instrumentation Tests

In order to run instrumentation tests, you must leave your device screen ON and
UNLOCKED. Otherwise, the test will timeout trying to launch an intent.
Optionally you can disable screen lock under Settings -> Security -> Screen Lock
-> None.

Next, you need to build the app, build your tests, and then run your tests
(which will install the APK under test and the test APK automatically).

Examples:

ContentShell tests:

```shell
# Build the code under test
ninja -C out/Release content_shell_apk

# Build the tests themselves
ninja -C out/Release content_shell_test_apk

# Run the test (will automagically install the APK under test and the test APK)
out/Release/bin/run_content_shell_test_apk [-vv]
```

ChromePublic tests:

```shell
# Build the code under test
ninja -C out/Release chrome_public_apk

# Build the tests themselves
ninja -C out/Release chrome_public_test_apk

# Run the test (will automagically install the APK under test and the test APK)
out/Release/bin/run_chrome_public_test_apk [-vv]
```

AndroidWebView tests:

```shell
ninja -C out/Release android_webview_apk
ninja -C out/Release android_webview_test_apk
out/Release/bin/run_android_webview_test_apk [-vv]
```

In order to run a subset of tests, use -f to filter based on test
class/method or -A/-E to filter using annotations.

Filtering examples:

```shell
# Run a test suite
out/Debug/bin/run_content_shell_test_apk

# Run a specific test class
out/Debug/bin/run_content_shell_test_apk -f AddressDetectionTest.*

# Run a specific test method
out/Debug/bin/run_content_shell_test_apk -f \
AddressDetectionTest#testAddressLimits

# Run a subset of tests by size (Smoke, SmallTest, MediumTest, LargeTest,
# EnormousTest)
out/Debug/bin/run_content_shell_test_apk -A Smoke

# Run a subset of tests by annotation, such as filtering by Feature
out/Debug/bin/run_content_shell_test_apk -A Feature=Navigation
```

You might want to add stars `*` to each as a regular expression, e.g.
`*`AddressDetectionTest`*`

## Running Blink Layout Tests

See [Layout Tests](testing/layout_tests.md).

## Running GPU tests

(e.g. the "Android Debug (Nexus 7)" bot on the chromium.gpu waterfall)

See http://www.chromium.org/developers/testing/gpu-testing for details. Use
`--browser=android-content-shell`. Examine the stdio from the test invocation on
the bots to see arguments to pass to `src/content/test/gpu/run_gpu_test.py`.
