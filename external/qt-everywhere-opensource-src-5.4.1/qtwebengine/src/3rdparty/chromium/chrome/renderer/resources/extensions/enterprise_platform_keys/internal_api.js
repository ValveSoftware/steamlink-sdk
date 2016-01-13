// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var binding = require('binding')
                  .Binding.create('enterprise.platformKeysInternal')
                  .generate();

exports.getTokens = binding.getTokens;
exports.generateKey = binding.generateKey;
exports.sign = binding.sign;
