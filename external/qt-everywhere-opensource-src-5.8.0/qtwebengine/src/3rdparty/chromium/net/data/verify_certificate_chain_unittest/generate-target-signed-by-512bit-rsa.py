#!/usr/bin/python
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Certificate chain with 1 intermediary and a trusted root. The target
certificate is signed using a weak RSA key (512-bit modulus), and so
verification is expected to fail."""

import common

# Self-signed root certificate (part of trust store).
root = common.create_self_signed_root_certificate('Root')

# Intermediary with a very weak key size (512-bit RSA).
intermediary = common.create_intermediary_certificate('Intermediary', root)
intermediary.generate_rsa_key(512)

# Target certificate.
target = common.create_end_entity_certificate('Target', intermediary)

chain = [target, intermediary]
trusted = [root]
time = common.DEFAULT_TIME
verify_result = False

common.write_test_file(__doc__, chain, trusted, time, verify_result)
