#!/usr/bin/python
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Certificate chain with 1 intermediary and a trusted root. The trusted root
is NOT self signed, however its issuer is not included in the chain or root
store. Verification is expected to succeed since the root is trusted."""

import common

shadow_root = common.create_self_signed_root_certificate('ShadowRoot')

# Non-self-signed root (part of trust store).
root = common.create_intermediary_certificate('Root', shadow_root)

# Intermediary certificate.
intermediary = common.create_intermediary_certificate('Intermediary', root)

# Target certificate.
target = common.create_end_entity_certificate('Target', intermediary)

chain = [target, intermediary]
trusted = [root]
time = common.DEFAULT_TIME
verify_result = True

common.write_test_file(__doc__, chain, trusted, time, verify_result)
