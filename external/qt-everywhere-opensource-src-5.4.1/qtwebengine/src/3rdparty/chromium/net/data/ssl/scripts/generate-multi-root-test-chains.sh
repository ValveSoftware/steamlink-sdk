#!/bin/sh

# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script generates two chains of test certificates:
#
#     1. A (end-entity) -> B -> C -> D (self-signed root)
#     2. A (end-entity) -> B -> C2 -> E (self-signed root)
#
# C and C2 have the same subject and keypair.
#
# We use these cert chains in CertVerifyProcChromeOSTest
# to ensure that multiple verification paths are properly handled.

try () {
  echo "$@"
  "$@" || exit 1
}

try rm -rf out
try mkdir out

echo Create the serial number files.
serial=1000
for i in B C C2 D E
do
  try /bin/sh -c "echo $serial > out/$i-serial"
  serial=$(expr $serial + 1)
done

echo Generate the keys.
try openssl genrsa -out out/A.key 2048
try openssl genrsa -out out/B.key 2048
try openssl genrsa -out out/C.key 2048
try openssl genrsa -out out/D.key 2048
try openssl genrsa -out out/E.key 2048

echo Generate the D CSR.
CA_COMMON_NAME="D Root CA" \
  CERTIFICATE=D \
  try openssl req \
    -new \
    -key out/D.key \
    -out out/D.csr \
    -config redundant-ca.cnf

echo D signs itself.
CA_COMMON_NAME="D Root CA" \
  try openssl x509 \
    -req -days 3650 \
    -in out/D.csr \
    -extensions ca_cert \
    -extfile redundant-ca.cnf \
    -signkey out/D.key \
    -out out/D.pem \
    -text

echo Generate the E CSR.
CA_COMMON_NAME="E Root CA" \
  CERTIFICATE=E \
  try openssl req \
    -new \
    -key out/E.key \
    -out out/E.csr \
    -config redundant-ca.cnf

echo E signs itself.
CA_COMMON_NAME="E Root CA" \
  try openssl x509 \
    -req -days 3650 \
    -in out/E.csr \
    -extensions ca_cert \
    -extfile redundant-ca.cnf \
    -signkey out/E.key \
    -out out/E.pem \
    -text

echo Generate the C2 intermediary CSR.
CA_COMMON_NAME="C CA" \
  CERTIFICATE=C2 \
  try openssl req \
    -new \
    -key out/C.key \
    -out out/C2.csr \
    -config redundant-ca.cnf

echo Generate the B and C intermediaries\' CSRs.
for i in B C
do
  CA_COMMON_NAME="$i CA" \
    CERTIFICATE="$i" \
    try openssl req \
      -new \
      -key "out/$i.key" \
      -out "out/$i.csr" \
      -config redundant-ca.cnf
done

echo D signs the C intermediate.
# Make sure the signer's DB file exists.
touch out/D-index.txt
CA_COMMON_NAME="D Root CA" \
  CERTIFICATE=D \
  try openssl ca \
    -batch \
    -extensions ca_cert \
    -in out/C.csr \
    -out out/C.pem \
    -config redundant-ca.cnf

echo E signs the C2 intermediate.
# Make sure the signer's DB file exists.
touch out/E-index.txt
CA_COMMON_NAME="E Root CA" \
  CERTIFICATE=E \
  try openssl ca \
    -batch \
    -extensions ca_cert \
    -in out/C2.csr \
    -out out/C2.pem \
    -config redundant-ca.cnf

echo C signs the B intermediate.
touch out/C-index.txt
CA_COMMON_NAME="C CA" \
  CERTIFICATE=C \
  try openssl ca \
    -batch \
    -extensions ca_cert \
    -in out/B.csr \
    -out out/B.pem \
    -config redundant-ca.cnf

echo Generate the A end-entity CSR.
try openssl req \
  -new \
  -key out/A.key \
  -out out/A.csr \
  -config ee.cnf

echo B signs A.
touch out/B-index.txt
CA_COMMON_NAME="B CA" \
  CERTIFICATE=B \
  try openssl ca \
    -batch \
    -extensions user_cert \
    -in out/A.csr \
    -out out/A.pem \
    -config redundant-ca.cnf

echo Create multi-root-chain1.pem
try /bin/sh -c "cat out/A.key out/A.pem out/B.pem out/C.pem out/D.pem \
    > ../certificates/multi-root-chain1.pem"

echo Create multi-root-chain2.pem
try /bin/sh -c "cat out/A.key out/A.pem out/B.pem out/C2.pem out/E.pem \
    > ../certificates/multi-root-chain2.pem"

