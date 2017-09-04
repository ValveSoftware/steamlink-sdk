# Android Studio

[TOC]

## Usage

```shell
build/android/gradle/generate_gradle.py --output-directory out-gn/Debug
```

This creates a project at `out-gn/Debug/gradle`. To create elsewhere:

```shell
build/android/gradle/generate_gradle.py --output-directory out-gn/Debug --project-dir my-project
```

By default, only common targets are generated. To customize the list of targets
to generate projects for:

```shell
build/android/gradle/generate_gradle.py --output-directory out-gn/Debug --target //some:target_apk --target //some/other:target_apk
```

For first-time Android Studio users:

 * Avoid running the setup wizard.
    * The wizard will force you to download unwanted SDK components to `//third_party/android_tools`.
    * To skip it, select "Cancel" when it comes up.

To import the project:

 * Use "Import Project", and select the directory containing the generated project.

You need to re-run `generate_gradle.py` whenever `BUILD.gn` files change.

 * After regenerating, Android Studio should prompt you to "Sync". If it doesn't, use:
    * Help -&gt; Find Action -&gt; Sync Project with Gradle Files


## How it Works

Android Studio integration works by generating `build.gradle` files based on GN
targets. Each `android_apk` and `android_library` target produces a separate
Gradle sub-project.

### Symlinks and .srcjars

Gradle supports source directories but not source files. However, some
`java/src/` directories in Chromium are split amonst multiple GN targets. To
accomodate this, the script detects such targets and creates a `symlinked-java/`
directory to point gradle at. Be warned that creating new files from Android
Studio within these symlink-based projects will cause new files to be created in
the generated `symlinked-java/` rather than the source tree where you want it.

*** note
** TLDR:** Always create new files outside of Android Studio.
***

Most generated .java files in GN are stored as `.srcjars`. Android Studio does
not have support for them, and so the generator script builds and extracts them
all to `extracted-srcjars/` subdirectories for each target that contains them.

*** note
** TLDR:** Always re-generate project files when `.srcjars` change (this
includes `R.java`).
***

## Android Studio Tips

 * Configuration instructions can be found [here](http://tools.android.com/tech-docs/configuration). One suggestions:
    * Launch it with more RAM: `STUDIO_VM_OPTIONS=-Xmx2048m /opt/android-studio-stable/bin/studio-launcher.sh`
 * If you ever need to reset it: `rm -r ~/.AndroidStudio*/`
 * Import Android style settings:
    * Help -&gt; Find Action -&gt; Code Style -&gt; Java -&gt; Manage -&gt; Import
       * Select `third_party/android_platform/development/ide/intellij/codestyles/AndroidStyle.xml`

### Useful Shortcuts

 * `Shift - Shift`: Search to open file or perform IDE action
 * `Ctrl + N`: Jump to class
 * `Ctrl + Shift + T`: Jump to test
 * `Ctrl + Shift + N`: Jump to file
 * `Ctrl + F12`: Jump to method
 * `Ctrl + G`: Jump to line
 * `Shift + F6`: Rename variable
 * `Ctrl + Alt + O`: Organize imports
 * `Alt + Enter`: Quick Fix (use on underlined errors)

### Building from the Command Line

Gradle builds can be done from the command-line after importing the project into
Android Studio (importing into the IDE causes the Gradle wrapper to be added).
This wrapper can also be used to invoke gradle commands.

    cd $GRADLE_PROJECT_DIR && bash gradlew

The resulting artifacts are not terribly useful. They are missing assets,
resources, native libraries, etc.

 * Use a [gradle daemon](https://docs.gradle.org/2.14.1/userguide/gradle_daemon.html) to speed up builds:
    * Add the line `org.gradle.daemon=true` to `~/.gradle/gradle.properties`, creating it if necessary.

## Status (as of Sept 21, 2016)

### What works

 * Tested with Android Studio v2.2.
 * Basic Java editing and compiling works.

### What doesn't work (yet) ([crbug](https://bugs.chromium.org/p/chromium/issues/detail?id=620034))

 * Better support for instrumentation tests (they are treated as non-test .apks right now)
 * Make gradle aware of resources and assets
 * Make gradle aware of native code via pointing it at the location of our .so
 * Add a mode in which gradle is responsible for generating `R.java`
 * Add support for native code editing
 * Make the "Make Project" button work correctly
