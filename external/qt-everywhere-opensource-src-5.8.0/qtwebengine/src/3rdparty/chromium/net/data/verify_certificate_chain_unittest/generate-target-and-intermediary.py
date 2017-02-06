#!/usr/bin/python
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Certificate chain with 1 intermediary and a trusted root. Verification is
expected to succeed."""

import common

# Self-signed root certificate (part of trust store).
root = common.create_self_signed_root_certificate('Root')

# Intermediary certificate.
intermediary = common.create_intermediary_certificate('Intermediary', root)

# Target certificate.
target = common.create_end_entity_certificate('Target', intermediary)

chain = [target, intermediary]
trusted = [root]
time = common.DEFAULT_TIME
verify_result = True

common.write_test_file(__doc__, chain, trusted, time, verify_result)
