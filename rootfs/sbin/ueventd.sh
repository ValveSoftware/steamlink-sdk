#!/bin/sh

umask 0
exec /sbin/ueventd "$@"
