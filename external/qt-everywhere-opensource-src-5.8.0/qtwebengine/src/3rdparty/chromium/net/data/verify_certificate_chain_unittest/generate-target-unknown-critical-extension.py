#!/usr/bin/python
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Certificate chain with 1 intermediary and a trusted root. The target
certificate has an unknown X.509v3 extension (OID=1.2.3.4) that is marked as
critical. Verifying this certificate chain is expected to fail because there is
an unrecognized critical extension."""

import common

# Self-signed root certificate (part of trust store).
root = common.create_self_signed_root_certificate('Root')

# Intermediary certificate.
intermediary = common.create_intermediary_certificate('Intermediary', root)

# Target certificate (has unknown critical extension).
target = common.create_end_entity_certificate('Target', intermediary)
target.get_extensions().add_property('1.2.3.4',
                                     'critical,DER:01:02:03:04')

chain = [target, intermediary]
trusted = [root]
time = common.DEFAULT_TIME
verify_result = False

common.write_test_file(__doc__, chain, trusted, time, verify_result)
