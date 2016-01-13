#!/bin/sh

# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script generates a set of test (end-entity, intermediate, root)
# certificates that can be used to test fetching of an intermediate via AIA.

try() {
  echo "$@"
  "$@" || exit 1
}

try rm -rf out
try mkdir out

try /bin/sh -c "echo 01 > out/2048-sha1-root-serial"
touch out/2048-sha1-root-index.txt

# Generate the key
try openssl genrsa -out out/2048-sha1-root.key 2048

# Generate the root certificate
CA_COMMON_NAME="Test Root CA" \
  try openssl req \
    -new \
    -key out/2048-sha1-root.key \
    -out out/2048-sha1-root.req \
    -config ca.cnf

CA_COMMON_NAME="Test Root CA" \
  try openssl x509 \
    -req -days 3650 \
    -in out/2048-sha1-root.req \
    -out out/2048-sha1-root.pem \
    -signkey out/2048-sha1-root.key \
    -extfile ca.cnf \
    -extensions ca_cert \
    -text

# Generate the leaf certificate requests
try openssl req \
  -new \
  -keyout out/expired_cert.key \
  -out out/expired_cert.req \
  -config ee.cnf

try openssl req \
  -new \
  -keyout out/ok_cert.key \
  -out out/ok_cert.req \
  -config ee.cnf

# Generate the leaf certificates
CA_COMMON_NAME="Test Root CA" \
  try openssl ca \
    -batch \
    -extensions user_cert \
    -startdate 060101000000Z \
    -enddate 070101000000Z \
    -in out/expired_cert.req \
    -out out/expired_cert.pem \
    -config ca.cnf

CA_COMMON_NAME="Test Root CA" \
  try openssl ca \
    -batch \
    -extensions user_cert \
    -days 3650 \
    -in out/ok_cert.req \
    -out out/ok_cert.pem \
    -config ca.cnf

try /bin/sh -c "cat out/ok_cert.key out/ok_cert.pem \
    > ../certificates/ok_cert.pem"
try /bin/sh -c "cat out/expired_cert.key out/expired_cert.pem \
    > ../certificates/expired_cert.pem"
try /bin/sh -c "cat out/2048-sha1-root.key out/2048-sha1-root.pem \
    > ../certificates/root_ca_cert.pem"

