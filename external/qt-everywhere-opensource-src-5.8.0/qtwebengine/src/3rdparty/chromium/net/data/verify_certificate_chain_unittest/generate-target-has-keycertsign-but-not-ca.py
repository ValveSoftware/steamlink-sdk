#!/usr/bin/python
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Certificate chain with 1 intermediary, a trusted root, and a target
certificate that is not a CA, and yet has the keyCertSign bit set. Verification
is expected to fail, since keyCertSign should only be asserted when CA is
true."""

import common

# Self-signed root certificate (part of trust store).
root = common.create_self_signed_root_certificate('Root')

# Intermediary certificate.
intermediary = common.create_intermediary_certificate('Intermediary', root)

# Target certificate (end entity but has keyCertSign bit set).
target = common.create_end_entity_certificate('Target', intermediary)
target.get_extensions().set_property('keyUsage',
    'critical,digitalSignature,keyEncipherment,keyCertSign')


chain = [target, intermediary]
trusted = [root]
time = common.DEFAULT_TIME
verify_result = False

common.write_test_file(__doc__, chain, trusted, time, verify_result)
