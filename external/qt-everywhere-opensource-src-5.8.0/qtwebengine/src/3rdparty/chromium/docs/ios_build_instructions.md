# iOS Build Instructions

**Note:** Upstreaming of iOS code is still a work in progress. In particular,
note that **it is not currently possible to build an actual Chromium app.**
Currently, the buildable binaries are ios\_web\_shell (a minimal wrapper around
the web layer), and various unit tests.

## Prerequisites

*   A Mac with a version of OS X capable of running the latest version
    of Xcode.
*   The latest version of [Xcode](https://developer.apple.com/xcode/),
    including the current iOS SDK.
*   The current version of the JDK (required for the closure compiler).
*   [depot\_tools](http://dev.chromium.org/developers/how-tos/install-depot-tools).

## Setting Up

### With GYP

In the directory where you are going to check out the code, create a
`chromium.gyp_env` to set the build to use iOS targets (and to use
hybrid builds; see [Building](#Building) below):

```shell
cat > chromium.gyp_env <<EOF
{
  "GYP_DEFINES": "OS=ios",
  "GYP_GENERATORS": "ninja,xcode-ninja",
}
EOF
```

If you aren't set up to sign iOS build products via a developer account,
you should instead use:

```shell
cat > chromium.gyp_env <<EOF
{
  "GYP_DEFINES": "OS=ios chromium_ios_signing=0",
  "GYP_GENERATORS": "ninja,xcode-ninja",
}
EOF
```

### With GN

Use `gn args out/Debug-iphonesimulator` (or replace
`out/Debug-iphonesimulator` with your chosen `out/` directory) to open up an
editor to set the following gn variables and regenerate:

```
# Set to true if you have a valid code signing key.
ios_enable_code_signing = false
target_os = "ios"
# Set to "x86", "x64", "arm", "armv7", "arm64". "x86" and "x64" will create a
# build to run on the iOS simulator (and set use_ios_simulator = true), all
# others are for an iOS device.
target_cpu = "x64"
# Release vs debug build.
is_debug = true
```

### API Keys

Before you build, you may want to
[install API keys](https://sites.google.com/a/chromium.org/dev/developers/how-tos/api-keys)
so that Chrome-integrated Google services work. This step is optional if you
aren't testing those features.

## Getting the Code

Next, [check out the
code](https://www.chromium.org/developers/how-tos/get-the-code), with:

```shell
fetch ios
```

## Building

Build the target you are interested in. The instructions above select
the ninja/Xcode hybrid mode, which uses ninja to do the actual build,
but provides a wrapper Xcode project that can be used to build targets
and navigate the source. (The Xcode project just shells out to ninja to
do the builds, so you can't actually inspect/change target-level
settings from within Xcode; this mode avoids generating a large tree of
Xcode projects, which leads to performance issues in Xcode). To build
with ninja (simulator and device, respectively):

```shell
ninja -C out/Debug-iphonesimulator All
ninja -C out/Debug-iphoneos All
```

To build with Xcode, open `build/all.ninja.xcworkspace`, and choose the
target you want to build.

You should always be able to build All, since targets are added there for iOS
only when they compile.

## Running

Any target that is built and runs on the bots (see [below](#Troubleshooting))
should run successfully in a local build. As of the time of writing, this is
only ios\_web\_shell and unit test targets—see the note at the top of this
page. Check the bots periodically for updates; more targets (new components)
will come on line over time.

To run in the simulator from the command line, you can use `iossim`. For
example, to run a debug build of ios\_web\_shell:

```shell
out/Debug-iphonesimulator/iossim out/Debug-iphonesimulator/ios_web_shell.app
```

## Converting an existing Mac checkout into an iOS checkout

If you want to convert your Mac checkout into an iOS checkout, follow the steps
below:

1.  Add `target_os = [ "ios" ]` to the bottom of your `chromium/.gclient`
file.

2.  For gyp, make sure you have the following in your
`chromium/chromium.gyp_env` file (removing the `chromium_ios_signing=0` if you
want to make developer-signed builds):

    ```json
    {
      "GYP_DEFINES" : "OS=ios chromium_ios_signing=0",
      "GYP_GENERATORS" : "ninja,xcode-ninja",
    }
    ```

    For gn, add the arguments specified [above](#With-GN) to your gn setup.

3.  Make sure to sync again to fetch the iOS specific dependencies and
regenerate build rules using:

    ```shell
    gclient sync
    ```

## Troubleshooting

If your build fails, check the iOS columns of [the Mac
waterfall](http://build.chromium.org/p/chromium.mac/console) (the last two) to
see if the bots are green. In general they should be, since failures on those
bots will close the tree.
