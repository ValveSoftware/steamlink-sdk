[TOC]

# Checkout
If you want to build the Android client then you will need to follow
instructions [here](https://www.chromium.org/developers/how-tos/android-build-instructions)
to sync Android related code as well.

# Using GN
Blimp only supports building using [GN](../../tools/gn/README.md). A quick
overview over how to use GN can be found in the GN
[quick start guide](../../tools/gn/docs/quick_start.md).

## Building

There are two different build configurations depending on what you want to
build:

### Android client

Create an out-directory and set the GN args:

```bash
mkdir -p out-android/Debug
echo "import(\"//build/args/blimp_client.gn\")" > out-android/Debug/args.gn
gn gen out-android/Debug
```

To build:

```bash
ninja -C out-android/Debug blimp
```

This will also generate an incremental APK, which you can install with this
command:

```bash
out-android/Debug/bin/install_blimp_apk_incremental
```

During development, it might be beneficial to put these two commands together
like this:

```bash
ninja -C out-android/Debug blimp && \
    out-android/Debug/bin/install_blimp_apk_incremental
```

To add your own build preferences:

```bash
gn args out-android/Debug
```

For example, you can build `x86` APK by adding `target_cpu = "x86"` to the `gn
args`.


### Engine

Create another out-directory and set the GN args:

```bash
mkdir -p out-linux/Debug
echo "import(\"//build/args/blimp_engine.gn\")" > out-linux/Debug/args.gn
gn gen out-linux/Debug
```

To build:

```bash
ninja -C out-linux/Debug blimp
```

To add your own build preferences

```bash
gn args out-linux/Debug
```

## Adding new build arguments

Adding new build arguments should be fairly rare. Arguments first need to be
[declared](../../tools/gn/docs/quick_start.md#Add-a-new-build-argument).

They can then be used to change how the binary is built or passed through to
code as a
[defines](../../tools/gn/docs/reference.md#defines_C-preprocessor-defines).

Finally the Blimp argument templates should be updated to reflect the
(non-default for Chrome) behavior desired by Blimp (see below).

## Updating bulid arguments in templates

Build argument templates exist for the client and engine at
[`build/args/blimp_client.gn`](../../build/args/blimp_client.gn) and
[`build/args/blimp_engine.gn`](../../build/args/blimp_engine.gn).

These can be updated as in the same manner as your personal `args.gn` files
to override default argument values.
