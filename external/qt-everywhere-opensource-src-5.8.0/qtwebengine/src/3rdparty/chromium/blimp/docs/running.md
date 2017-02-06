# Running Blimp
[TOC]

See [build](build.md) for instructions on how to build Blimp.

## Android Client

### Installing the client

Install the Blimp APK with the following:

```bash
./build/android/adb_install_apk.py $(PRODUCT_DIR)/apks/Blimp.apk
```

This is equivalent to just running:

```bash
adb install $(PRODUCT_DIR)/apks/Blimp.apk
```

### Setting up command line flags

Set up any command line flags with:

```bash
./build/android/adb_blimp_command_line --your-flag-here
```

To see your current command line, run `adb_blimp_command_line` without any
arguments.

The Blimp client reads command line arguments from the file
`/data/local/blimp-command-line` and the above script reads/writes that file.
Typical format of the file is `chrome --your-flag-here`. So one can use `adb`
directly to create the file:

```bash
echo 'chrome --engine-ip=10.0.2.2 --engine-port=25467 --engine-transport=tcp' \
  '--blimp-client-token-path=/data/data/org.chromium.blimp/blimp_client_token' \
  '--vmodule="*=1""' > /tmp/blimp-command-line
adb push /tmp/blimp-command-line /data/local/blimp-command-line
adb shell start chmod 0664 /data/local/blimp-command-line
```

### Instructing client to connect to specific host

To have the client connect to a custom engine use the `--engine-ip`,
`--engine-port`, and `--engine-transport` flags. The possible valid
values for `--engine-transport` are 'tcp' and 'ssl'.
An example valid endpoint would be
`--engine-ip=127.0.0.1 --engine-port=25467 --engine-transport=tcp`.

SSL-encrypted connections take an additional flag
`--engine-cert-path` which specifies a path to a PEM-encoded certificate
file (e.g. `--engine-cert-path=/path_on_device_to/file.pem`). Remember to also
copy the file to the device when using this option.

### Requesting the complete page from the engine
The engine sends partially rendered content to the client. To request the
complete page from the engine, use the `--download-whole-document` flag on the
client.

### Specifying the client auth token file
The client needs access to a file containing a client auth token. One should
make sure this file is available by pushing it onto the device before running
the client. One can do this by running the following command:

```bash
adb push /path/to/blimp_client_token \
  /data/data/org.chromium.blimp/blimp_client_token
```

To have the client use the given client auth token file, use the
`--blimp-client-token-path` flag (e.g.
`--blimp-client-token-path=/data/data/org.chromium.blimp/blimp_client_token`)

An example of a client token file is
[test_client_token](https://code.google.com/p/chromium/codesearch#chromium/src/blimp/test/data/test_client_token).

### Start the Client
Run the Blimp APK with:

```bash
./build/android/adb_run_blimp_client
```
The script under the cover uses adb to start the application:

```bash
adb shell am start -a android.intent.action.VIEW -n org.chromium.blimp/org.chromium.blimp.BlimpRendererActivity
```

### Connecting to an Engine running on a workstation
To run the engine on a workstation and make your Android client connect to it,
you need to forward a port from the Android device to the host, and also
instruct the client to connect using that port on its localhost address.

#### Port forwarding
If you are running the engine on your workstation and are connected to the
client device via USB, you'll need remote port forwarding to allow the Blimp
client to talk to your computer.

##### Option A
Follow the
[remote debugging instructions](https://developer.chrome.com/devtools/docs/remote-debugging)
to get started. You'll probably want to remap 25467 to "localhost:25467".
*Note* This requires the separate `Chrome` application to be running on the
device. Otherwise you will not see the green light for the port forwarding.

##### Option B
If you are having issues with using the built-in Chrome port forwarding, you can
also start a new shell and keep the following command running:

```bash
./build/android/adb_reverse_forwarder.py --debug -v 25467 25467
```

### Running the client in an Android emulator
Running the client using an Android emulator is similar to running it on device.
Here are a few gotchas:

* Build an APK matching the emulator cpu type. The emulator cpu type is most
  likely x86, while the default Android build cpu type is ARM. Follow the
  Android build instructions above to change the CPU type.

* Ensure that the emulator is running at least Android Marshmallow so that
  fonts are rendered correctly. Also ensure that you have the latest Play
  services binaries.

* Some of scripts under `build/android` will fail to run as it uses `adb scp`.
  Follow the instruction above to use `adb` directly.

* To connect to an Engine running on the host machine, you should use
  `10.0.2.2` as `engine-ip` instead of `127.0.0.1` as `127.0.0.1` will refer to
  the emulator itself. There is no need of set up
  [port forwarding](#Port-forwarding) when this approach is used.

## Linux Client

The Linux client is used for development purpose while the Android client is
shipped. The Linux client is built as part of the `blimp` target.
To run it with local logging enabled, execute:

```bash
./out-linux/Debug/blimp_shell \
  --user-data-dir=/tmp/blimpclient \
  --enable-logging=stderr \
  --vmodule="*=1"
```

## Running the engine

### In a container
For running the engine in a container, see [container](container.md).

### On a workstation
The following flags are required to start an Engine instance:

* `--blimp-client-token-path=$PATH`: Path to a file containing a nonempty
  token string. If this is not present, the engine will fail to boot.
* `--use-remote-compositing`: Ensures that the renderer uses the remote
  compositor.
* `--disable-cached-picture-raster`: Ensures that rasterized content is not
  destroyed before serialization.
* `--android-fonts-path=$PATH`: Path to where the fonts are located.
  Typically this would be `out-linux/Debug/gen/third_party/blimp_fonts`.
* `--disable-remote-fonts`: Disables downloading of custom web fonts in the
  renderer.

#### Typical invocation

One can start the engine using these flags:

```bash
out-linux/Debug/blimp_engine_app \
  --android-fonts-path=out-linux/Debug/gen/third_party/blimp_fonts \
  --blimp-client-token-path=/tmp/blimpengine-token \
  --enable-logging=stderr \
  --vmodule="blimp*=1"
```
