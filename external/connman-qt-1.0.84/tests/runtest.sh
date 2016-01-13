#!/bin/bash

TEST_NAME="${1}"

cd "$(dirname "$0")" || exit 1

if ! [[ -n ${TEST_NAME} && !(${TEST_NAME} =~ /) && -x ${TEST_NAME}.bin ]]
then
    echo "Usage: $0 <test-name> [test-args...]" >&2
    exit 1
fi

eval `dbus-launch --sh-syntax`
trap "kill ${DBUS_SESSION_BUS_PID}" EXIT
./${TEST_NAME}.bin "${@}"
