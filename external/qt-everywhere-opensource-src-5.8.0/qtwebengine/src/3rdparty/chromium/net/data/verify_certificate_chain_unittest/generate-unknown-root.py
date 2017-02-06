#!/usr/bin/python
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Certificate chain with 1 intermediary, but the root is not in trust store.
Verification is expected to fail because the final intermediary (Intermediary)
does not chain to a known root."""

import common

# Self-signed root certificate, which is NOT added to the trust store.
root = common.create_self_signed_root_certificate('Root')

# Intermediary certificate.
intermediary = common.create_intermediary_certificate('Intermediary', root)

# Target certificate.
target = common.create_end_entity_certificate('Target', intermediary)

chain = [target, intermediary]
trusted = []  # Note that this lacks |root|
time = common.DEFAULT_TIME
verify_result = False

common.write_test_file(__doc__, chain, trusted, time, verify_result)
