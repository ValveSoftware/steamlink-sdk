# Testing

Blimp only supports building using [GN](../../tools/gn/README.md), and only
supports building for Android and Linux.  See [building](build.md) for general
GN setup.

## Testing on Android

Run the following command to build the Android tests:

```bash
ninja -C out-android/Debug blimp_tests
```

### Running the Java instrumentation tests

Install the Blimp APK with the following:

```bash
./build/android/adb_install_apk.py $(PRODUCT_DIR)/apks/Blimp.apk
```

Run the Java instrumentation tests (with an optional test filter) with the
following:

```bash
$(PRODUCT_DIR)/bin/run_blimp_test_apk [ -f DummyTest#* ]
```

### Testing on Linux

Run the following command to build the Linux tests:

```bash
ninja -C out-linux/Debug blimp_tests
```

There are no tests that run on Linux yet.
