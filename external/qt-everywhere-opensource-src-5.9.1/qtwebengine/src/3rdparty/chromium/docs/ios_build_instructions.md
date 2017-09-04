# iOS Build Instructions

**Note:** Upstreaming of iOS code is still a work in progress. In particular,
note that **it is not currently possible to build an actual Chromium app.**
Currently, the buildable binaries are ios\_web\_shell (a minimal wrapper around
the web layer), and various unit tests.

## Prerequisites

*   A Mac running 10.11+.
*   [Xcode] 8.0+.
*   [depot\_tools].
*   The current version of the JDK (required for the closure compiler).

## Getting the source

To checkout the source, use `fetch ios` command from [depot\_tools] in a new
empty directory.

```shell
# You can use a different location for your checkout of Chromium on iOS
# by updating this variable. All shell snippets will refer to it.
CHROMIUM_IOS="$HOME/chromium_ios"
mkdir "$CHROMIUM_IOS"
cd "$CHROMIUM_IOS"
fetch ios
```

## Setting up

Chromium on iOS is built using the [Ninja](ninja_build.md) tool and
the [Clang](clang.md) compiler. See both of those pages for further details on
how to tune the build.

Before you build, you may want to [install API keys](api-keys) so that
Chrome-integrated Google services work. This step is optional if you aren't
testing those features.

### Quick setup

To setup the repository for building Chromium on iOS code, it is recommended
to use the `src/ios/build/tools/setup-gn.py` script that creates a Xcode
workspace configured to build the different targets for device and simulator.

```shell
cd "$CHROMIUM_IOS/src"
ios/build/tools/setup-gn.py
open out/build/all.xcworkspace
```

You can customize the build by editing the file `$HOME/.setup-gn` (create it
if it does not exists).  Look at `src/ios/build/tools/setup-gn.config` for
available configuration options.

From this point, you can either build from Xcode or from the command-line
using `ninja`. The script `setup-gn.py` creates sub-directories named
`out/${configuration}-${platform}`, so for a `Debug` build for simulator
use:

```shell
ninja -C out/Debug-iphonesimulator gn_all
```

Note: you need to run `setup-gn.py` script every time one of the `BUILD.gn`
file is updated (either by you or after rebasing). If you forget to run it,
the list of targets and files in the Xcode solution may be stale.

### Advanced setup

You can run `gn` manually to configure the build yourself. In that case,
refer to [mac build instructions] for help on how to do that.

To build for iOS, you have to set `target_os` to `"ios"`. Please also note
that `is_component_build` is not supported when building for iOS and must
be set to `false`.

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

## Troubleshooting

If your build fails, check the iOS columns of [the Mac
waterfall](http://build.chromium.org/p/chromium.mac/console) (the last two) to
see if the bots are green. In general they should be, since failures on those
bots will close the tree.

[Xcode]: https://developer.apple.com/xcode
[depot\_tools]: https://dev.chromium.org/developers/how-tos/depottools
[Ninja]: ninja.md
[Clang]: clang.md
[api-keys]: https://sites.google.com/a/chromium.org/dev/developers/how-tos/api-keys
[mac build instructions]: mac_build_instructions.md
