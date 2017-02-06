# Running the engine in a Docker container

For local development and testing, you can run the engine in a Docker
container.

The steps are:

1. Bundle the engine and its dependencies.

1. Build a Docker image.

1. Create a Docker container.


## About Docker

To get a high-level overview of Docker, please read [Understand the
architecture](https://docs.docker.com/introduction/understanding-docker/).
Optional reading includes reference guides for
[`docker run`](https://docs.docker.com/reference/run/) and
[Dockerfile](https://docs.docker.com/reference/builder/).


### Installation

For Googlers running Goobuntu wanting to install Docker, see
[go/installdocker](https://goto.google.com/installdocker). For other
contributors using Ubuntu, see [official Docker
installation instructions](https://docs.docker.com/installation/ubuntulinux/).


## Bundle Engine

The `blimp/engine:blimp_engine_bundle` build target will bundle the engine and
its dependencies into a tarfile, which can be used to build a Docker image.
This target is always built as part of the top-level `blimp/blimp` meta-target.

### Manually checking dependencies

`gen/engine-manifest.txt` is a list of the engine's runtime
dependencies. This list is automatatically generated in the build, but can
be manually replicated for debugging and investigation. Use
`blimp/tools/generate-target-manifest.py` to manually generate the manifest
after building blimp target to generate the runtime deps file:

```bash
./blimp/tools/generate-target-manifest.py \
    --blacklist blimp/tools/engine-manifest-blacklist.txt \
    --output out-linux/Debug/engine-manifest.txt \
    --runtime-deps-file out-linux/Debug/gen/blimp-engine.runtime_deps
```

You can compare the output at `out-linux/Debug/engine-manifest.txt` with the
generated target `out-linux/Debug/gen/engine-manifest.txt`.

## Build Docker Image

Using the tarfile you can create a Docker image:

```bash
docker build -f Dockerfile.base -t base - < ./out-linux/Debug/blimp_engine_bundle.tar.gz
docker build -t blimp_engine - < ./out-linux/Debug/blimp_engine_bundle.tar.gz
```

## Running the Engine in a Docker Container

After building the Docker image you can launch the engine inside the Docker
container.

### Setting up an Environment

A little prep work is necessary to enable the engine to start as it requires a
few files that are not provided by the container. You need:

*   A directory (`$CONFIG_DIR`) with permissions of 0755 (ie. world accessable)
*   `$CONFIG_DIR/stunnel.pem`: A PEM encoded file with a private key and a
    public certificate. Permissions should be set to 644.
*   `$CONFIG_DIR/client_token`: A file with a non-empty string used as the
    client token (the shared secret between the client and the engine).
    Permissions should also be set to 644. See [running](running.md) for how
    to get the default token from the source code.

This setup step is only required once and can be reused for all the rest of the
runs of the engine.

### Running the Engine

Once the `$CONFIG_DIR` is set up, you can launch the engine in the Docker
container:

```bash
docker run -v $CONFIG_DIR:/engine/data -p 443:25466 blimp_engine
```
You can also pass additional flags:

```bash
docker run ... blimp_engine --with-my-flags
```
See the [blimp engine `Dockerfile`](../engine/Dockerfile) to find out what flags
are passed by default.
