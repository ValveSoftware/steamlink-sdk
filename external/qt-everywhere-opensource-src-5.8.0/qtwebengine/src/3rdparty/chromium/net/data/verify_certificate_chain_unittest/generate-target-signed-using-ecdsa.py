#!/usr/bin/python
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Certificate chain with a trusted root using RSA, and intermediary using EC,
and a target certificate using RSA. Verification is expected to succeed."""

import common

# Self-signed root certificate (part of trust store), using RSA.
root = common.create_self_signed_root_certificate('Root')

# Intermediary using an EC key for the P-384 curve.
intermediary = common.create_intermediary_certificate('Intermediary', root)
intermediary.generate_ec_key('secp384r1')

# Target certificate contains an RSA key (but is signed using ECDSA).
target = common.create_end_entity_certificate('Target', intermediary)

chain = [target, intermediary]
trusted = [root]
time = common.DEFAULT_TIME
verify_result = True

common.write_test_file(__doc__, chain, trusted, time, verify_result)
