// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEFINE_PRIVATE_KEY_TYPE
#error "Please define DEFINE_PRIVATE_KEY_TYPE before including this file."
#endif

DEFINE_PRIVATE_KEY_TYPE(RSA, 0)
DEFINE_PRIVATE_KEY_TYPE(DSA, 1)
DEFINE_PRIVATE_KEY_TYPE(ECDSA, 2)
DEFINE_PRIVATE_KEY_TYPE(INVALID, 255)
