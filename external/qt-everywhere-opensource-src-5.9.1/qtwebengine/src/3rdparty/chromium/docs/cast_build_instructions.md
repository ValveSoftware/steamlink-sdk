# Cast Build Instructions

**Note**: it is **not possible** to build a binary functionally
equivalent to a Chromecast. This is to build a single-page content
embedder with similar functionality to Cast products.

## Prerequisites

*   See the [Linux build prerequisites](https://chromium.googlesource.com/chromium/src/+/master/docs/linux_build_instructions_prerequisites.md)

## Setting Up

*   Cast Linux build only: [Linux build
    setup](https://chromium.googlesource.com/chromium/src/+/master/docs/linux_build_instructions.md)
    is sufficient.
*   Cast Linux and Android builds: follow the [Android build
    setup](https://www.chromium.org/developers/how-tos/android-build-instructions)
    instructions.

## Building/running cast\_shell (Linux)

```shell
gn gen out/Debug --args='is_chromecast=true is_debug=true'
ninja -C out/Debug cast_shell
```

```shell
out/Debug/cast_shell --ozone-platform=x11 http://google.com
```

## Building/running cast\_shell\_apk (Android)

```shell
gn gen out/Debug --args='is_chromecast=true target_os="android" is_debug=true'
ninja -C out/Debug cast_shell_apk
```

```shell
adb install out/Debug/apks/CastShell.apk
adb shell am start -d "http://google.com" org.chromium.chromecast.shell/.CastShellActivity
```