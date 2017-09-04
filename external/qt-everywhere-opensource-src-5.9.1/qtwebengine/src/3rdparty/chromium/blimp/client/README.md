# Blimp Client

This document contains documentation for the blimp client code organization.

For a quick guide for how to add Java files to an existing directory, see
[Adding Java files](#adding-java-files) below.

[TOC]

## Code Organization

The blimp client has two main directories to be used for code, `core` and
`public`. Embedders should only ever have to deal with the `public` directory.

There are sub-directories resembling the ones in `core` for parts that needs to
be exposed to embedders, such as `BlimpContents`.

All Java-code is put in the `public/android` directory, with a package
organization that resembles the one in `core`.

Within `//blimp/client`, the `visibility` feature of GN is heavily used to
ensure that embedders and dependencies are correctly set up. Currently, the
`app` directory is not fully extracted out as an embedder, so it does sometime
depend directly on code in `core`, but that should change when the migration to
`core` is finished.

### The core directory

The `core` directory is to be used for all business logic code, and is an
internal concept of the client code.

There are sub-directories for logically separate parts, such as `contents` for
code that relate to the contents of a web page. All Android code is put into
their respective directories, so Android and Java-code related to `contents`
is put in `contents/android`.

Each of the sub-directories have their own `BUILD.gn` file, which includes
targets for both C++ and Java.

*   `//blimp/client/core/`
    *   `compositor/` Code related to the Chrome Compositor.
    *   `contents/` Code related to the contents of a web page.
    *   `contents/android/` JNI bridges and Java-code for `contents`.
    *   **`context/` Code related to the context (`BlimpClientContext`), which
        is the core functionality used by all embedders.**
    *   `context/android` JNI bridges and Java-code for `context`.
    *   `session/` Code related to the session with the engine.

Most code in `core` do not need any Java counterparts unless the embedder is
written for Android and it is typically only needed for things that relate to
the embedder UI.

#### Actual and dummy implementations of core

The `core` directory has two implementations of the public API, a dummy one
and a real one. The default is to use the dummy API, but an embedder can choose
to enable full blimp support by setting the GN arguments `enable_blimp_client`
to `true`.

Basically only the implementation of `BlimpClientContext` has been split out
into two parts (both in C++ and Java), and the choice of which backing
implementation to be used is selected by the `enable_blimp_client` flag. These
two implementations live in `//blimp/client/context`.

### The public directory

The `public` directory represents the public API of the blimp client, to be
used by the embedders.

Embedders should be able to depend on `//blimp/client/public` for C++ code,
and for Java they should depend on `//blimp/client/public:public_java`. This
automatically pulls in the dependencies on the code in `core`.

*   `//blimp/client/public/`
    *   `android/` All Android-related code, including Java-code. This includes
        also code from all the sub-directories such as `contents`.
    *   `contents/` Code from `//blimp/client/core/contents/` that needs to be
        exposed to embedders.
    *   `session/` Code from `//blimp/client/core/session/` that needs to be
        exposed to embedders.

### Other directories

#### The app directory

The `app` directory represents the directory that contains embedder code for
a Blimp application on its own. Under a transition period, this directory
also contains code that depends directly on `core`, until everything can be
updated to depend only on `public`.

#### The feature directory

The `feature` directory is from the old directory organization, and all the
content of this will move over to the `core` directory. The feature-specific
code will move together with the usage of the feature itself, such as
`core/contents`.

#### The session directory

The `session` directory is from the old directory organization, and all the
content of this will move over to the `core/session` directory.

#### The support directory

The `support` directory is a directory providing help to embedders. This
typically includes a default implementation of an interface from
`//blimp/client/public` or other helpful tools.

#### The test directory

The `test` directory contains tools helpful for testing client code.

### Adding Java files {#adding-java-files}

Most of Blimp is written in C++, but some parts have to be written in Java for
the Android platform. For those parts of the code, it is required to add an
`android` sub-directory, and put JNI-bridges and Java files within there.

This will mostly happen in a sub-directory of `core`, so the following section
explains how to add a Java file with JNI hooks for the imaginary sub-directory `//blimp/client/core/foo`. Conceptually, it's adding a Java-version of the C++
class `Foo`, that lives in `//blimp/client/core/foo/foo.[cc|h]`.

*   The file Foo.java should be added at:
    `//blimp/client/core/foo/android/java/src/org/chromium/blimp/core/foo/Foo.java`

*   The JNI-bridge should live in:
    `//blimp/client/core/foo/android/foo_android.[cc|h]`

*   Add the JNI-bridge JNI registration to:
     `//blimp/client/core/context/android/blimp_jni_registrar.cc`

*   Add this to the top of `//blimp/client/core/foo/BUILD.gn`:

    ```python
    if (is_android) {
      import("//build/config/android/config.gni")
      import("//build/config/android/rules.gni")
    }
    ```

*   Add the following targets to `//blimp/client/core/foo/BUILD.gn`:

    ```python
    android_library("foo_java") {
      visibility = [ "//blimp/client/*" ]

      java_files = [
        "android/java/src/org/chromium/blimp/core/foo/Foo.java",
      ]

      deps = [
        ...
      ]
    }

    generate_jni("jni_headers") {
      visibility = [ ":*" ]

      sources = [
        "android/java/src/org/chromium/blimp/core/foo/Foo.java",
      ]

      jni_package = "blimp/client/core/foo"
    }
    ```

*   Edit the target `//blimp/client/core/foo:foo` to add this Android-part:

    ```python
    source_set("foo") {

      ...

      if (is_android) {
       sources += [
         "android/foo.cc",
         "android/foo.h",
       ]

       deps += [ ":jni_headers" ]
      }
    }
    ```

*   Add `//blimp/client/core/foo:foo_java` as a dependency in
    `//blimp/client/core:core_java`.
