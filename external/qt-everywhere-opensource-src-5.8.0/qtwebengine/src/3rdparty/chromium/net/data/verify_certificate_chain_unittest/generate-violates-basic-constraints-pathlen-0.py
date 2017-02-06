#!/usr/bin/python
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Certificate chain with 2 intermediaries. The first intermediary has a basic
constraints path length of 0, so it is a violation for it to have a subordinate
intermediary."""

import common

# Self-signed root certificate (part of trust store).
root = common.create_self_signed_root_certificate('Root')

# Intermediary with pathlen 0
intermediary1 = common.create_intermediary_certificate('Intermediary1', root)
intermediary1.get_extensions().set_property('basicConstraints',
                                            'critical,CA:true,pathlen:0')

# Another intermediary (with the same pathlen restriction)
intermediary2 = common.create_intermediary_certificate('Intermediary2',
                                                       intermediary1)
intermediary2.get_extensions().set_property('basicConstraints',
                                            'critical,CA:true,pathlen:0')

# Target certificate.
target = common.create_end_entity_certificate('Target', intermediary2)

chain = [target, intermediary2, intermediary1]
trusted = [root]
time = common.DEFAULT_TIME
verify_result = False

common.write_test_file(__doc__, chain, trusted, time, verify_result)
