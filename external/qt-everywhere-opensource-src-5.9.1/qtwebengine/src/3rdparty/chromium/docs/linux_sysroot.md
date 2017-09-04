# Linux sysroot images

The chromium build system for Linux will (by default) use a sysroot image
rather than building against the libraries installed on the host system.
This serves several purposes.  Firstly, it ensures that binaries will run on all
supported linux systems independent of the packages installed on the build
machine.  Secondly, it makes the build more hermetic, preventing issues that
arise for variations among developers' systems.

The sysroot consists of a minimal installation of Debian/stable (or old-stable)
to ensure maximum compatibility.  Pre-built sysroot images are stored in
Google Cloud Storage and downloaded during `gclient runhooks`

## Installing the sysroot images

Installation of the sysroot is performed by
`build/linux/sysroot_scripts/install-sysroot.py`.

This script can be run manually but is normally run as part of gclient
hooks. When run from hooks this script in a no-op on non-linux platforms.

## Rebuilding the sysroot image

The pre-built sysroot images occasionally needs to be rebuilt.  For example,
when security updates to debian are released, or when a new package is needed by
the chromium build.

### Rebuilding

To rebuild the images (without any changes) run the following commands:

    $ cd build/linux/sysroot_scripts
    $ ./sysroot-creator-wheezy.sh BuildSysrootAll

The above command will rebuild the sysroot for all architectures. To build
just one architecture use `BuildSysroot<arch>`.  Run the script with no
arguments for a list of possible architectures.  For example:

    $ ./sysroot-creator-wheezy.sh BuildSysrootAmd64

This command on its own should be a no-op and produce an image identical to
the one on Google Cloud Storage.

### Updating existing package list

To update packages to the latest versions run:

    $ ./sysroot-creator-wheezy.sh UpdatePackageListsAll

This command will update the package lists that are stored alongside the script.
If no packages have changed then this script will have no effect.

### Adding new packages

To add a new package, edit the `sysroot-creator-wheezy.sh` script and modify
the `DEBIAN_PACKAGES` list, then run the update step above
(`UpdatePackageListsAll`).

### Uploading new images

To upload images to Google Cloud Storage run the following command:

    $ ./sysroot-creator-wheezy.sh UploadSysrootAll <SHA1>

Here you should use the SHA1 of the git revision at which the images were
created.

Uploading new images to Google Clound Storage requires write permission on the
`chrome-linux-sysroot` bucket.

### Rolling the sysroot version used by chromium

Once new images have been uploaded the `install-sysroot.py` script needs to be
updated to reference the new versions.  This process is manual and involves
updating the `REVISION` and `SHA1SUM` variables in the script.
