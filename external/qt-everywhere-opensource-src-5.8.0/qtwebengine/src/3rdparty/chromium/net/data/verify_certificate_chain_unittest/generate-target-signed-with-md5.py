#!/usr/bin/python
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Certificate chain with an intermediary that uses MD5 to sign the target
certificate. This is expected to fail because MD5 is too weak."""

import common

# Self-signed root certificate (part of trust store).
root = common.create_self_signed_root_certificate('Root')

# Intermediary.
intermediary = common.create_intermediary_certificate('Intermediary', root)

# Target certificate.
target = common.create_end_entity_certificate('Target', intermediary)
target.set_signature_hash('md5')

chain = [target, intermediary]
trusted = [root]
time = common.DEFAULT_TIME
verify_result = False

common.write_test_file(__doc__, chain, trusted, time, verify_result)
