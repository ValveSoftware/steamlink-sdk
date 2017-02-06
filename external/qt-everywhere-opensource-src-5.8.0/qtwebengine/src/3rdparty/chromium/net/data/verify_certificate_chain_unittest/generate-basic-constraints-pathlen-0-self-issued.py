#!/usr/bin/python
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Certificate chain with 2 intermediaries. The first intermediary has a basic
constraints path length of 0. The second one is self-issued so does not count
against the path length."""

import common

# Self-signed root certificate (part of trust store).
root = common.create_self_signed_root_certificate('Root')

# Intermediary with pathlen 0
intermediary1 = common.create_intermediary_certificate('Intermediary', root)
intermediary1.get_extensions().set_property('basicConstraints',
                                            'critical,CA:true,pathlen:0')

# Another intermediary (with the same pathlen restriction).
# Note that this is self-issued but NOT self-signed.
intermediary2 = common.create_intermediary_certificate('Intermediary',
                                                       intermediary1)
intermediary2.get_extensions().set_property('basicConstraints',
                                            'critical,CA:true,pathlen:0')

# Target certificate.
target = common.create_end_entity_certificate('Target', intermediary2)

chain = [target, intermediary2, intermediary1]
trusted = [root]
time = common.DEFAULT_TIME
verify_result = True

common.write_test_file(__doc__, chain, trusted, time, verify_result)
