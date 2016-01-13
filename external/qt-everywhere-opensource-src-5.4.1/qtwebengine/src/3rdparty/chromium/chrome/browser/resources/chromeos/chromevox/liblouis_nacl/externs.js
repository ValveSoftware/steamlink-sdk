// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Additional externs for the Closure Compiler.
 * @author jbroman@google.com (Jeremy Roman)
 * @externs
 */

/**
 * <embed> element which wraps a Native Client module.
 * @constructor
 * @extends HTMLEmbedElement
 */
function NaClEmbedElement() {}

/**
 * Exposed when Native Client is present.
 * @param {*} message Message to post.
 */
NaClEmbedElement.prototype.postMessage = function(message) {};
