#!/usr/bin/python
# Copyright (c) 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Certificate chain with 2 intermediaries and one end entity certificate. The
root certificate has a pathlen:1 restriction so this is an invalid chain."""

import common

# Self-signed root certificate (part of trust store).
root = common.create_self_signed_root_certificate('Root')
root.get_extensions().set_property('basicConstraints',
                                   'critical,CA:true,pathlen:1')

# Intermediary 1 (no pathlen restriction).
intermediary1 = common.create_intermediary_certificate('Intermediary1', root)

# Intermediary 2 (no pathlen restriction).
intermediary2 = common.create_intermediary_certificate('Intermediary2',
                                                       intermediary1)

# Target certificate.
target = common.create_end_entity_certificate('Target', intermediary2)

chain = [target, intermediary2, intermediary1]
trusted = [root]
time = common.DEFAULT_TIME
verify_result = False

common.write_test_file(__doc__, chain, trusted, time, verify_result)
