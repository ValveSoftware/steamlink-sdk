#!/usr/bin/python
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Certificate chain where the target has an incorrect signature. Everything
else should check out, however the digital signature contained in the target
certificate is wrong."""

import common

# Self-signed root certificate (part of trust store).
root = common.create_self_signed_root_certificate('Root')

# Intermediary certificate to include in the certificate chain.
intermediary = common.create_intermediary_certificate('Intermediary', root)

# Actual intermediate that was used to sign the target certificate. It has the
# same subject as expected, but a different RSA key from the certificate
# included in the actual chain.
wrong_intermediary = common.create_intermediary_certificate('Intermediary',
                                                            root)

# Target certificate, signed using |wrong_intermediary| NOT |intermediary|.
target = common.create_end_entity_certificate('Target', wrong_intermediary)

chain = [target, intermediary]
trusted = [root]
time = common.DEFAULT_TIME
verify_result = False

common.write_test_file(__doc__, chain, trusted, time, verify_result)
