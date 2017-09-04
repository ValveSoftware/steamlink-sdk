# Mac Build Instructions

[TOC]

Google employee? See [go/building-chrome-mac](https://goto.google.com/building-chrome-mac) for extra tips.

## Prerequisites

*   A Mac running 10.9+.
*   [Xcode](https://developer.apple.com/xcode) 7.3+.
*   [depot\_tools](http://dev.chromium.org/developers/how-tos/depottools).
*   The OSX 10.10 SDK. Run
    ```
    ls `xcode-select -p`/Platforms/MacOSX.platform/Developer/SDKs
    ```
    to check whether you have it.  Building with the 10.11 SDK works too, but
    the releases currently use the 10.10 SDK.

## Getting the code

[Check out the source code](https://www.chromium.org/developers/how-tos/get-the-code)
using Git.

Before checking out, go to the
[waterfall](http://build.chromium.org/buildbot/waterfall/) and check that the
source tree is open (to avoid pulling a broken tree).

The path to the build directory should not contain spaces (e.g. not
`~/Mac OS X/chromium`), as this will cause the build to fail. This includes your
drive name, the default "Macintosh HD2" for a second drive has a space.

## Building

Chromium on OS X is built using the [Ninja](ninja_build.md) tool and
the [Clang](clang.md) compiler. See both of those pages for further details on
how to tune the build.

Run

    gn gen out/gn

to generate build files (replace "gn" in "out/gn" with whatever you like), and
then run

    ninja -C out/gn chrome

to build.  You can edit out/gn/args.gn to configure the build.

Before you build, you may want to
[install API keys](https://sites.google.com/a/chromium.org/dev/developers/how-tos/api-keys)
so that Chrome-integrated Google services work. This step is optional if you
aren't testing those features.

## Faster builds

Full rebuilds are about the same speed in Debug and Release, but linking is a
lot faster in Release builds.

Put

    is_debug = false

in your args.gn to do a release build.

Put

    is_component_build = true

in your args.gn to build many small dylibs instead of a single large executable.
This makes incremental builds much faster, at the cost of producing a binary
that opens less quickly.  Component builds work in both debug and release.

Put

    symbol_level = 1

in your args.gn to disable debug symbols altogether.  This makes both full
rebuilds and linking faster (at the cost of not getting symbolized backtraces
in gdb).

You might also want to [install ccache](ccache_mac.md) to speed up the build.

## Running

All build output is located in the `out` directory (in the example above,
`~/chromium/src/out`).  You can find the applications at
`gn/Content Shell.app` and `gn/Chromium.app`.

## Unit Tests

We have several unit test targets that build, and tests that run and pass. A
small subset of these is:

*   `unit_tests` from `chrome/chrome.gyp`
*   `base_unittests` from `base/base.gyp`
*   `net_unittests` from `net/net.gyp`
*   `url_unittests` from `url/url.gyp`

When these tests are built, you will find them in the `out/gn`
directory. You can run them from the command line:

    ~/chromium/src/out/gn/unit_tests


## Coding

According to the
[Chromium style guide](http://dev.chromium.org/developers/coding-style) code is
[not allowed to have whitespace on the ends of lines](https://google.github.io/styleguide/cppguide.html#Horizontal_Whitespace).

Run `git cl format` after committing to your local branch and before uploading
to clang-format your code.

## Debugging

Good debugging tips can be found
[here](http://dev.chromium.org/developers/how-tos/debugging-on-os-x). If you
would like to debug in a graphical environment, rather than using `lldb` at the
command line, that is possible without building in Xcode. See
[Debugging in Xcode](http://www.chromium.org/developers/how-tos/debugging-on-os-x/building-with-ninja-debugging-with-xcode)
for information on how.

## Contributing

Once you’re comfortable with building Chromium, check out
[Contributing Code](http://dev.chromium.org/developers/contributing-code) for
information about writing code for Chromium and contributing it.

## Using Xcode-Ninja Hybrid

While using Xcode is unsupported, gn supports a hybrid approach of using ninja
for building, but Xcode for editing and driving compilation.  Xcode is still
slow, but it runs fairly well even **with indexing enabled**.  Most people
build in the Terminal and write code with a text editor though.

With hybrid builds, compilation is still handled by ninja, and can be run by the
command line (e.g. ninja -C out/gn chrome) or by choosing the chrome target
in the hybrid workspace and choosing build.

To use Xcode-Ninja Hybrid pass --ide=xcode to `gn gen`

```shell
gn gen out/gn --ide=xcode
```

Open it:

```shell
open out/gn/ninja/all.xcworkspace
```

You may run into a problem where http://YES is opened as a new tab every time
you launch Chrome. To fix this, open the scheme editor for the Run scheme,
choose the Options tab, and uncheck "Allow debugging when using document
Versions Browser". When this option is checked, Xcode adds
`--NSDocumentRevisionsDebugMode YES` to the launch arguments, and the `YES` gets
interpreted as a URL to open.

If you have problems building, join us in `#chromium` on `irc.freenode.net` and
ask there. As mentioned above, be sure that the
[waterfall](http://build.chromium.org/buildbot/waterfall/) is green and the tree
is open before checking out. This will increase your chances of success.

## Improving performance of `git status`

`git status` is used frequently to determine the status of your checkout.  Due
to the number of files in Chromium's checkout, `git status` performance can be
quite variable.  Increasing the system's vnode cache appears to help.  By
default, this command:

    sysctl -a | egrep kern\..*vnodes

Outputs `kern.maxvnodes: 263168` (263168 is 257 * 1024).  To increase this
setting:

    sudo sysctl kern.maxvnodes=$((512*1024))

Higher values may be appropriate if you routinely move between different
Chromium checkouts.  This setting will reset on reboot, the startup setting can
be set in `/etc/sysctl.conf`:

    echo kern.maxvnodes=$((512*1024)) | sudo tee -a /etc/sysctl.conf

Or edit the file directly.

If your `git --version` reports 2.6 or higher, the following may also improve
performance of `git status`:

    git update-index --untracked-cache

## Xcode license agreement

If you're getting the error

```
Agreeing to the Xcode/iOS license requires admin privileges, please re-run as root via sudo.
```

the Xcode license hasn't been accepted yet which (contrary to the message) any
user can do by running:

    xcodebuild -license

Only accepting for all users of the machine requires root:

    sudo xcodebuild -license
