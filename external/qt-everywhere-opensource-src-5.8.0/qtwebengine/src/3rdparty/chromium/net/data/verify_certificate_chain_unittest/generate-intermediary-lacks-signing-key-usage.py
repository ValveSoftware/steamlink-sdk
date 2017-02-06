#!/usr/bin/python
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Certificate chain with 1 intermediary and a trusted root. The intermediary
contains a keyUsage extension, HOWEVER it does not contain the keyCertSign bit.
Hence validation is expected to fail."""

import common

# Self-signed root certificate (part of trust store).
root = common.create_self_signed_root_certificate('Root')

# Intermediary that is missing keyCertSign.
intermediary = common.create_intermediary_certificate('Intermediary', root)
intermediary.get_extensions().set_property('keyUsage',
    'critical,digitalSignature,keyEncipherment')

# Target certificate.
target = common.create_end_entity_certificate('Target', intermediary)

chain = [target, intermediary]
trusted = [root]
time = common.DEFAULT_TIME
verify_result = False

common.write_test_file(__doc__, chain, trusted, time, verify_result)
