#!/bin/bash
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This a simple script to make running Mojo shell easier (on Linux).

DIRECTORY="$(dirname "$0")"/../../out/Debug
PORT=$(($RANDOM % 8192 + 2000))

do_help() {
  cat << EOF
Usage: $(basename "$0") [-d DIRECTORY] [-|--] <mojo_shell arguments ...>

DIRECTORY defaults to $DIRECTORY.

Example:
  $(basename "$0") mojo:mojo_sample_app
EOF
}

kill_http_server() {
  echo "Killing SimpleHTTPServer ..."
  kill $HTTP_SERVER_PID
  wait $HTTP_SERVER_PID
}

while [ $# -gt 0 ]; do
  case "$1" in
    -h|--help)
      do_help
      exit 0
      ;;
    -d)
      shift
      if [ $# -eq 0 ]; then
        do_help
        exit 1
      fi
      DIRECTORY="$1"
      ;;
    --show-bash-alias)
      echo alias\ mojosh\=\'\"\$\(pwd\ \|\ sed\ \'\"\'\"\'s\/\\\(\.\*\\\/src\\\
\)\.\*\/\\1\/\'\"\'\"\'\)\/mojo\/tools\/mojosh\.sh\"\'
      exit 0
      ;;
    # Separate arguments to mojo_shell (e.g., in case you want to pass it -d).
    -|--)
      shift
      break
      ;;
    *)
      break
      ;;
  esac
  shift
done

echo "Base directory: $DIRECTORY"

echo "Running SimpleHTTPServer in directory $DIRECTORY/lib on port $PORT"
cd $DIRECTORY/lib || exit 1
python -m SimpleHTTPServer $PORT &
# Kill the HTTP server on exit (even if the user kills everything using ^C).
HTTP_SERVER_PID=$!
trap kill_http_server EXIT
cd ..

echo "Running:"
echo "./mojo_shell --origin=http://127.0.0.1:$PORT --disable-cache $*"
./mojo_shell --origin=http://127.0.0.1:$PORT --disable-cache $*
