#!/bin/sh
#
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Sanitize environment.
set -e
export LANG=C
export LC_ALL=C

PROGDIR=$(dirname "$0")
PROGNAME=$(basename "$0")

# Defaults
VERBOSE=1

. "$PROGDIR/openssl-chromium.config"

# Print error message and exit script.
# $1+: Error message.
panic () {
  echo "ERROR: $@"
  exit 1
}

# $1: Variable name
# Output: variable value.
var_value () {
  # NOTE: Don't use 'echo' here, it is sensitive to options like -n.
  eval printf \"%s\\n\" \$$1
}

# Print a message if verbosity is sufficiently high.
# $1: Verbosity threshold, only if '$VERBOSE > $1' does this print.
# $2+: Message.
dump_n () {
  local LEVEL=$1
  shift
  if [ "$VERBOSE" -gt "$LEVEL" ]; then
    printf "%s\n" "$@"
  fi
}

# Print a message, unless --quiet was used.
dump () {
  dump_n 0 "$@"
}

# Print a message if --verbose was used.
log () {
  dump_n 1 "$@"
}

# Print a message if --verbose --verbose was used.
log2 () {
  dump_n 2 "$@"
}

# Run a command silently, unless --verbose is used.
# More specifically:
#  - By default, this runs the command but redirects its stdout/stderr
#    to /dev/null to avoid printing anything.
#  - If --verbose is used, this prints the command's name, and stderr
#    will not be redirected.
#  - If '--verbose --verbose' is used, this prints the commands and its
#    complete output.
# $1+: Command
# Return: Command status
run () {
  if [ "$VERBOSE" -gt 1 ]; then
    echo "COMMAND: $@"
  fi
  case $VERBOSE in
    0)
        "$@" > /dev/null 2>&1
        ;;
    1)
        "$@" > /dev/null
        ;;
    *)
        "$@"
        ;;
  esac
}

# Support cleaning up stuff when the script exits, even in case of
# error.
_ALL_CLEANUPS=

clean_atexit () {
  local CLEANUPS CLEANUP
  CLEANUPS=$_ALL_CLEANUPS
  _ALL_CLEANUPS=
  for CLEANUP in $CLEANUPS; do
    ($CLEANUP)
  done
  exit $1
}

trap "clean_atexit 0" EXIT
trap "clean_atexit \$?" HUP INT QUIT TERM

# Add a cleanup function to the list of cleanups that will be run when
# the script exits.
atexit () {
    # Prepend to ensure that the cleanup steps are performed in reverse
    # order or registration.
    _ALL_CLEANUPS="$* $_ALL_CLEANUPS"
}

# Support code to write into a gyp file
_GYP_MARGIN=""

# Increment margin of gyp printer.
incr_gyp_margin () {
  _GYP_MARGIN="$_GYP_MARGIN  "
}

decr_gyp_margin () {
  _GYP_MARGIN=$(echo "$_GYP_MARGIN" | cut --bytes=3-)
}

print_gyp () {
  printf "%s%s\n" "$_GYP_MARGIN" "$@"
}

# This prints a list variable definition in a gyp file.
# $1: Variable name (e.g. 'openssl_common_defines')
# $2+: List items (e.g. defines)
print_gyp_variable () {
  local VARNAME=$1
  local VALUE
  shift
  print_gyp "'$VARNAME': ["
  for VALUE; do
    print_gyp "  '$VALUE',"
  done
  print_gyp "],"
}

# Same as print_gyp_variable, but for source file lists, this
# prepends openssl/ as required by the Chromium build to each item
# in the list.
# $1: Variable name (e.g. 'openssl_common_sources')
# $2+: List items (source file names).
print_gyp_source_variable () {
  local VARNAME=$1
  local VALUE
  shift
  print_gyp "'$VARNAME': ["
  for VALUE; do
    print_gyp "  'openssl/$VALUE',"
  done
  print_gyp "],"
}

# Print usage instructions.
usage () {
    echo \
"Usage: $PROGNAME [options]

This script is used to regenerate the content of the Chromium
third_party/openssl/ directory according to the configuration file
named 'openssl-chromium.config'.

In particular, it will perform the following steps:

  1) Download the Android sources from the AOSP git servers.

  2) Add Chromium-specific patches to the Android source tree.
     (they must be under patches.chromium/ in $PROGDIR).

  3) Download a versioned openssl package from the official OpenSSL
     servers, and check its MD5. The version is taken from the
     'openssl.version' file in the Android source tree.

  4) Run the Android 'import_openssl.sh' script that rebuilds all sources
     from a clean slate.

  5) Generate the 'openssl.gypi' that contains gyp-specific declarations
     for the library.

  6) Generate 64-bit compatible opensslconf.h header.

Valid options are the following (defaults are in brackets):

  --help|-h|-?          Display this message.
  --aosp-git=<url>      Change git source for Android repository.
                        [$ANDROID_OPENSSL_GIT_SOURCE]
  --aosp-commit=<name>  Specify git commit or branch name [$ANDROID_OPENSSL_GIT_COMMIT]
  --temp-dir=<path>     Specify temporary directory, will not be cleaned.
                        [<random-temp-file-cleaned-on-exit>]
  --verbose             Increase verbosity.
  --quiet               Decrease verbosity.
"
  exit 1
}

# Parse command-line.
DO_HELP=

for OPT; do
  OPTARG=$()
  case $OPT in
    --help|-h|-?)
        DO_HELP=true
        ;;
    --aosp-commit=*)
        ANDROID_OPENSSL_GIT_COMMIT=${OPT#--aosp-commit=}
        if [ -z "$ANDROID_OPENSSL_GIT_COMMIT" ]; then
            panic "Missing option value: $OPT"
        fi
        ;;
    --aosp-git=*)
        ANDROID_OPENSSL_GIT_SOURCE=${OPT#--aosp-git=}
        if [ -z "$ANDROID_OPENSSL_GIT_SOURCE" ]; then
            panic "Missing option value: $OPT"
        fi
        ;;
    --temp-dir=*)
        TEMP_DIR=${OPT#--temp-dir=}
        if [ -z "$TEMP_DIR" ]; then
            panic "Missing option value: $OPT"
        fi
        ;;
    --quiet)
        VERBOSE=$(( $VERBOSE - 1 ))
        ;;
    --verbose)
        VERBOSE=$(( $VERBOSE + 1 ))
        ;;
    -*)
        panic "Invalid option '$OPT', see --help for details."
        ;;
    *)
        panic "This script doesn't take parameters. See --help for details."
        ;;
  esac
done

if [ "$DO_HELP" ]; then
  usage
fi

# Create temporary directory. Ensure it's always cleaned up on exit.
if [ -z "$TEMP_DIR" ]; then
  TEMP_DIR=$(mktemp -d)
  clean_tempdir () {
    rm -rf "$TEMP_DIR"
  }
  atexit clean_tempdir
  log "Temporary directory created: $TEMP_DIR"
else
  log "Using user-provided temp directory: $TEMP_DIR"
fi

GIT_FLAGS=
case $VERBOSE in
  0|1)
    GIT_CLONE_FLAGS="--quiet"
    GIT_CHECKOUT_FLAGS="--quiet"
    CURL_FLAGS="-s"
    ;;
  2)
    GIT_CLONE_FLAGS=""
    GIT_CHECKOUT_FLAGS=""
    CURL_FLAGS=""
    ;;
  *)
    GIT_CLONE_FLAGS="--verbose"
    GIT_CHECKOUT_FLAGS=""
    CURL_FLAGS=""
    ;;
esac

BUILD_DIR=$TEMP_DIR/build
mkdir -p "$BUILD_DIR" && rm -rf "$BUILD_DIR"/*

# Download the Android sources.
ANDROID_SRC_DIR=$BUILD_DIR/android-openssl
dump "Downloading Android sources"
log "Downloading branch $ANDROID_OPENSSL_GIT_COMMIT from: $ANDROID_OPENSSL_GIT_SOURCE"
(
  run mkdir -p $ANDROID_SRC_DIR
  run cd $ANDROID_SRC_DIR
  run git clone $GIT_CLONE_FLAGS $ANDROID_OPENSSL_GIT_SOURCE .
  run git checkout $GIT_CHECKOUT_FLAGS $ANDROID_OPENSSL_GIT_COMMIT
  run rm -rf .git
)

# Apply chromium-specific patches located in patches.chromium
CHROMIUM_PATCHES_DIR=$PROGDIR/patches.chromium
if [ ! -d "$CHROMIUM_PATCHES_DIR" ]; then
  dump "No Chromium-specific patches to apply."
else
  dump "Applying Chromium-specific patches:"
  CHROMIUM_PATCHES=$(/bin/ls $CHROMIUM_PATCHES_DIR/*.patch 2>/dev/null)
  for CHROMIUM_PATCH in $CHROMIUM_PATCHES; do
    dump "Applying: $CHROMIUM_PATCH"
    (cd $ANDROID_SRC_DIR && run patch -p1) < $CHROMIUM_PATCH
  done
fi

# Get the openssl version
. $ANDROID_SRC_DIR/openssl.version
if [ -z "$OPENSSL_VERSION" ]; then
  panic "Could not find OPENSSL_VERSION definition from $ANDROID_SRC_DIR!"
fi
dump "Found OpenSSL version: $OPENSSL_VERSION"

# Download OpenSSL package
DOWNLOAD_DIR=$BUILD_DIR/download
mkdir -p "$DOWNLOAD_DIR"

OPENSSL_PACKAGE=openssl-$OPENSSL_VERSION.tar.gz
dump "Downloading $OPENSSL_PACKAGE from $OPENSSL_TAR_SOURCE"
run curl $CURL_FLAGS -o $DOWNLOAD_DIR/$OPENSSL_PACKAGE $OPENSSL_TAR_SOURCE/$OPENSSL_PACKAGE
run curl $CURL_FLAGS -o $DOWNLOAD_DIR/$OPENSSL_PACKAGE.md5 $OPENSSL_TAR_SOURCE/$OPENSSL_PACKAGE.md5

OPENSSL_SHA1_DOWNLOADED=$(sha1sum $DOWNLOAD_DIR/$OPENSSL_PACKAGE | cut -d" " -f1)
OPENSSL_SHA1_EXPECTED=$OPENSSL_TAR_SHA1
if [ "$OPENSSL_SHA1_DOWNLOADED" != "$OPENSSL_SHA1_EXPECTED" ]; then
  echo "ERROR: Content mismatch for downloaded OpenSSL package:"
  echo "       Downloaded SHA-1: $OPENSSL_SHA1_DOWNLOADED"
  echo "       Expected SHA-1  : $OPENSSL_SHA1_EXPECTED"
  exit 1
fi
dump "Checking content of downloaded package: ok"

# The import_openssl.sh script will really remove the existing 'openssl'
# directory and replace it with something completely new. This is a problem
# when using subversion because this also gets rid of all .svn
# subdirectories. This makes it impossible to commit the right set of
# changes with "gcl commit".
#
# To work-around this, copy all the .svn subdirectories into a temporary
# tarball, which will be extracted after the import process.
#
dump "Saving .svn subdirectories"
SVN_LIST_FILE=$BUILD_DIR/svn-subdirs
run find . -type d -name ".svn" > $SVN_LIST_FILE
SAVED_SVN_TARBALL=$BUILD_DIR/saved-svn-subdirs.tar.gz
run tar czf $SAVED_SVN_TARBALL -T $SVN_LIST_FILE

# Re-run the import_openssl.sh script.
dump "Re-running the 'import_openssl.sh' script to reconfigure all sources."
(
  cd $ANDROID_SRC_DIR
  run ./import_openssl.sh import $DOWNLOAD_DIR/$OPENSSL_PACKAGE
)

dump "Copying new Android sources to final location."
clean_openssl_new () {
  rm -rf "$PROGDIR/openssl.new"
}
atexit clean_openssl_new

run cp -rp "$ANDROID_SRC_DIR" "$PROGDIR/openssl.new"
run mv "$PROGDIR/openssl" "$PROGDIR/openssl.old"
run mv "$PROGDIR/openssl.new" "$PROGDIR/openssl"
run rm -rf "$PROGDIR/openssl.old"

dump "Restoring .svn subdirectores"
run tar xzf $SAVED_SVN_TARBALL

# Extract list of source files or compiler defines from openssl.config
# variable definition. This assumes that the lists are in variables that
# are named as <prefix><suffix> or <prefix><suffix><arch>.
#
# A few examples:
#   get_gyp_list "FOO BAR" _SOURCES
#     -> returns '$FOO_SOURCES $BAR_SOURCES'
#
#   get_gyp_list FOO _SOURCES_ "arm x86"
#     -> returns '$FOO_SOURCES_arm $FOO_SOURCES_x86"
#
#   get_gyp_list "FOO BAR" _SOURCES_ "arm x86"
#     -> returns '$FOO_SOURCES_arm $FOO_SOURCES_x86 $BAR_SOURCES_arm $BAR_SOURCES_x86'
#
# $1: list of variable prefixes
# $2: variable suffix
# $3: optional list of architectures.
get_gyp_list () {
  local ALL_PREFIXES="$1"
  local SUFFIX="$2"
  local ALL_ARCHS="$3"
  local LIST PREFIX ARCH
  for PREFIX in $ALL_PREFIXES; do
    if [ "$ALL_ARCHS" ]; then
      for ARCH in $ALL_ARCHS; do
        LIST="$LIST $(var_value ${PREFIX}${SUFFIX}${ARCH})"
      done
    else
      LIST="$LIST $(var_value ${PREFIX}${SUFFIX})"
    fi
  done
  echo "$LIST"
}

generate_gyp_file () {
  echo "# Auto-generated file - DO NOT EDIT"
  echo "# To regenerate - run import_from_android.sh."
  echo "# See 'import_from_android.sh --help' for details."

  local ALL_PREFIXES="OPENSSL_CRYPTO OPENSSL_SSL"
  local ALL_ARCHS="arm mips x86 x86_64 mac_ia32"
  local PREFIX ARCH LIST

  print_gyp "{"
  incr_gyp_margin

    print_gyp "'variables': {"
    incr_gyp_margin

      # First, the common sources and defines
      print_gyp_source_variable "openssl_common_sources" \
          $(get_gyp_list "$ALL_PREFIXES" _SOURCES)

      print_gyp_variable "openssl_common_defines" \
          $(get_gyp_list "$ALL_PREFIXES" _DEFINES)

      # Now, conditions section with add architecture-specific sub-sections.
      for ARCH in $ALL_ARCHS; do
        # Convert ARCH to gyp-specific architecture name
        case $ARCH in
          x86)
            GYP_ARCH=ia32
            ;;
          x86_64)
            GYP_ARCH=x64
            ;;
          *)
            GYP_ARCH=$ARCH
            ;;
        esac

        print_gyp_source_variable "openssl_${ARCH}_source_excludes" \
            $(get_gyp_list "$ALL_PREFIXES" _SOURCES_EXCLUDES_ $ARCH)

        print_gyp_source_variable "openssl_${ARCH}_sources" \
            $(get_gyp_list "$ALL_PREFIXES" _SOURCES_ $ARCH)

        print_gyp_variable "openssl_${ARCH}_defines" \
            $(get_gyp_list "$ALL_PREFIXES" _DEFINES_ $ARCH)

      done # for ARCH

    decr_gyp_margin
    print_gyp "}"  # variables

  decr_gyp_margin
  print_gyp "}"  # top-level dict.
}

dump "Generating 64-bit configuration header file."
mkdir -p $PROGDIR/config/x64/openssl/
sed \
  -e 's|^#define RC4_INT unsigned char|#define RC4_INT unsigned int|g' \
  -e 's|^#define BN_LLONG|#undef BN_LLONG|g' \
  -e 's|^#define THIRTY_TWO_BIT|#undef THIRTY_TWO_BIT|g' \
  -e 's|^#undef SIXTY_FOUR_BIT_LONG|#define SIXTY_FOUR_BIT_LONG|g' \
  -e 's|^#define BF_PTR|#undef BF_PTR|g' \
  $PROGDIR/openssl/include/openssl/opensslconf.h \
  > $PROGDIR/config/x64/openssl/opensslconf.h

dump "Generating OS X 32-bit configuration header file."
mkdir -p $PROGDIR/config/mac/ia32/openssl/
sed \
  -e '4a#ifndef OPENSSL_SYSNAME_MACOSX\n# define OPENSSL_SYSNAME_MACOSX\n#endif' \
  -e 's|^#define RC4_INT unsigned char|#define RC4_INT unsigned int|g' \
  -e 's|^#define DES_LONG unsigned int|#define DES_LONG unsigned long|g' \
  $PROGDIR/openssl/include/openssl/opensslconf.h \
  > $PROGDIR/config/mac/ia32/openssl/opensslconf.h

dump "Generating .gypi file."
. $ANDROID_SRC_DIR/openssl.config
generate_gyp_file > $PROGDIR/openssl.gypi.new
run mv $PROGDIR/openssl.gypi $PROGDIR/openssl.gypi.old
run mv $PROGDIR/openssl.gypi.new $PROGDIR/openssl.gypi
run rm $PROGDIR/openssl.gypi.old

dump "Done."
