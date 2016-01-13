#!/bin/bash
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This a simple script to make building/testing Mojo components easier (on
# Linux).

# TODO(vtl): Maybe make the test runner smart and not run unchanged test
# binaries.
# TODO(vtl) Maybe also provide a way to pass command-line arguments to the test
# binaries.

do_help() {
  cat << EOF
Usage: $(basename "$0") [command|option ...]

command should be one of:
  build - Build.
  test - Run unit tests (does not build).
  perftest - Run perf tests (does not build).
  pytest - Run Python unit tests.
  gyp - Run gyp for mojo (does not sync), with clang.
  sync - Sync using gclient (does not run gyp).
  show-bash-alias - Outputs an appropriate bash alias for mojob. In bash do:
      \$ eval \`mojo/tools/mojob.sh show-bash-alias\`

option (which will only apply to following commands) should be one of:
  Build/test options (specified before build/test/perftest):
    --debug - Build/test in Debug mode.
    --release - Build/test in Release mode.
    --debug-and-release - Build/test in both Debug and Release modes (default).
  Compiler options (specified before gyp):
    --clang - Use clang (default).
    --gcc - Use gcc.
  Component options:
    --shared Build components as shared libraries (default).
    --static Build components as static libraries.
  Mojo in chromium/content (crbug.com/353602):
    --use-mojo - Enabled (default).
    --no-use-mojo - Disabled.

Note: It will abort on the first failure (if any).
EOF
}

do_build() {
  echo "Building in out/$1 ..."
  ninja -C "out/$1" mojo || exit 1
}

do_unittests() {
  echo "Running unit tests in out/$1 ..."
  mojo/tools/test_runner.py mojo/tools/data/unittests "out/$1" \
      mojob_test_successes || exit 1
}

do_perftests() {
  echo "Running perf tests in out/$1 ..."
  "out/$1/mojo_public_system_perftests" || exit 1
}

do_pytests() {
  python mojo/tools/run_mojo_python_tests.py || exit 1
}

do_gyp() {
  local gyp_defines="$(make_gyp_defines)"
  echo "Running gyp with GYP_DEFINES=$gyp_defines ..."
  GYP_DEFINES="$gyp_defines" build/gyp_chromium mojo/mojo.gyp || exit 1
}

do_sync() {
  # Note: sync only (with hooks, but no gyp-ing).
  GYP_CHROMIUM_NO_ACTION=1 gclient sync || exit 1
}

# Valid values: Debug, Release, or Debug_and_Release.
BUILD_TEST_TYPE=Debug_and_Release
should_do_Debug() {
  test "$BUILD_TEST_TYPE" = Debug -o "$BUILD_TEST_TYPE" = Debug_and_Release
}
should_do_Release() {
  test "$BUILD_TEST_TYPE" = Release -o "$BUILD_TEST_TYPE" = Debug_and_Release
}

# Valid values: clang or gcc.
COMPILER=clang
# Valid values: shared or static.
COMPONENT=shared
make_gyp_defines() {
  local options=()
  # Always include these options.
  options+=("use_aura=1")
  case "$COMPILER" in
    clang)
      options+=("clang=1")
      ;;
    gcc)
      options+=("clang=0")
      ;;
  esac
  case "$COMPONENT" in
    shared)
      options+=("component=shared_library")
      ;;
    static)
      options+=("component=static_library")
      ;;
  esac
  echo ${options[*]}
}

# We're in src/mojo/tools. We want to get to src.
cd "$(realpath "$(dirname "$0")")/../.."

if [ $# -eq 0 ]; then
  do_help
  exit 0
fi

for arg in "$@"; do
  case "$arg" in
    # Commands -----------------------------------------------------------------
    help|--help)
      do_help
      exit 0
      ;;
    build)
      should_do_Debug && do_build Debug
      should_do_Release && do_build Release
      ;;
    test)
      should_do_Debug && do_unittests Debug
      should_do_Release && do_unittests Release
      ;;
    perftest)
      should_do_Debug && do_perftests Debug
      should_do_Release && do_perftests Release
      ;;
    pytest)
      do_pytests
      ;;
    gyp)
      do_gyp
      ;;
    sync)
      do_sync
      ;;
    show-bash-alias)
      # You want to type something like:
      #   alias mojob=\
      #       '"$(pwd | sed '"'"'s/\(.*\/src\).*/\1/'"'"')/mojo/tools/mojob.sh"'
      # This is quoting hell, so we simply escape every non-alphanumeric
      # character.
      echo alias\ mojob\=\'\"\$\(pwd\ \|\ sed\ \'\"\'\"\'s\/\\\(\.\*\\\/src\\\)\
\.\*\/\\1\/\'\"\'\"\'\)\/mojo\/tools\/mojob\.sh\"\'
      ;;
    # Options ------------------------------------------------------------------
    --debug)
      BUILD_TEST_TYPE=Debug
      ;;
    --release)
      BUILD_TEST_TYPE=Release
      ;;
    --debug-and-release)
      BUILD_TEST_TYPE=Debug_and_Release
      ;;
    --clang)
      COMPILER=clang
      ;;
    --gcc)
      COMPILER=gcc
      ;;
    --shared)
      COMPONENT=shared
      ;;
    --static)
      COMPONENT=static
      ;;
    *)
      echo "Unknown command \"${arg}\". Try \"$(basename "$0") help\"."
      exit 1
      ;;
  esac
done
