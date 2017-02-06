# Windows Build Instructions

## Common checkout instructions

This page covers Windows-specific setup and configuration. The
[general checkout
instructions](http://dev.chromium.org/developers/how-tos/get-the-code) cover
installing depot tools and checking out the code via git.

## Setting up Windows

You must set your Windows system locale to English, or else you may get
build errors about "The file contains a character that cannot be
represented in the current code page."

### Setting up the environment for Visual Studio

You must build with Visual Studio 2015 Update 2; no other version is
supported.

You must have Windows 7 x64 or later. x86 OSs are unsupported.

## Getting the compiler toolchain

Follow the appropriate path below:

### Open source contributors

As of March 11, 2016 Chromium requires Visual Studio 2015 to build.

Install Visual Studio 2015 Update 2 or later - Community Edition
should work if its license is appropriate for you. Use the Custom Install option
and select:

- Visual C++, which will select three sub-categories including MFC
- Universal Windows Apps Development Tools > Tools
- Universal Windows Apps Development Tools > Windows 10 SDK (10.0.10586)

You must have the 10586 SDK installed or else you will hit compile errors such
as redefined macros.

Run `set DEPOT_TOOLS_WIN_TOOLCHAIN=0`, or set that variable in your
global environment.

Compilation is done through ninja, **not** Visual Studio.

### Google employees

Run: `download_from_google_storage --config` and follow the
authentication instructions. **Note that you must authenticate with your
@google.com credentials**, not @chromium.org. Enter "0" if asked for a
project-id.

Run: `gclient sync` again to download and install the toolchain automatically.

The toolchain will be in `depot_tools\win_toolchain\vs_files\<hash>`, and windbg
can be found in `depot_tools\win_toolchain\vs_files\<hash>\win_sdk\Debuggers`.

If you want the IDE for debugging and editing, you will need to install
it separately, but this is optional and not needed to build Chromium.

## Using the Visual Studio IDE

If you want to use the Visual Studio IDE, use the `--ide` command line
argument to `gn gen` when you generate your output directory (as described on
the [get the code](http://dev.chromium.org/developers/how-tos/get-the-code)
page):

```gn gen --ide=vs out\Default
devenv out\Default\all.sln
```

GN will produce a file `all.sln` in your build directory. It will internally
use Ninja to compile while still allowing most IDE functions to work (there is
no native Visual Studio compilation mode). If you manually run "gen" again you
will need to resupply this argument, but normally GN will keep the build and
IDE files up to date automatically when you build.

The generated solution will contain several thousand projects and will be very
slow to load. Use the `--filters` argument to restrict generating project files
for only the code you're interested in, although this will also limit what
files appear in the project explorer. A minimal solution that will let you
compile and run Chrome in the IDE but will not show any source files is:

```gn gen --ide=vs --filters=//chrome out\Default```

There are other options for controlling how the solution is generated, run `gn
help gen` for the current documentation.

## Performance tips

1.  Have a lot of fast CPU cores and enough RAM to keep them all busy.
    (Minimum recommended is 4-8 fast cores and 16-32 GB of RAM)
2.  Reduce file system overhead by excluding build directories from
    antivirus and indexing software.
3.  Store the build tree on a fast disk (preferably SSD).
4.  If you are primarily going to be doing debug development builds, you
    should use the component build. Set the [build
    arg](https://www.chromium.org/developers/gn-build-configuration)
    `is_component_build = true`.
    This will generate many DLLs and enable incremental linking, which makes
    linking **much** faster in Debug.

Still, expect build times of 30 minutes to 2 hours when everything has to
be recompiled.
