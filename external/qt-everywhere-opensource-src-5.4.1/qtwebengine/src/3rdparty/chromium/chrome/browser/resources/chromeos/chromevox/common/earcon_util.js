// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Earcon utils.
 */

goog.provide('cvox.EarconUtil');

goog.require('cvox.AbstractEarcons');
goog.require('cvox.AriaUtil');
goog.require('cvox.DomUtil');

/**
 * Returns the id of an earcon to play along with the description for a node.
 *
 * @param {Node} node The node to get the earcon for.
 * @return {number?} The earcon id, or null if none applies.
 */
cvox.EarconUtil.getEarcon = function(node) {
  var earcon = cvox.AriaUtil.getEarcon(node);
  if (earcon != null) {
    return earcon;
  }

  switch (node.tagName) {
    case 'BUTTON':
      return cvox.AbstractEarcons.BUTTON;
    case 'A':
      if (node.hasAttribute('href')) {
        return cvox.AbstractEarcons.LINK;
      }
      break;
    case 'IMG':
      if (cvox.DomUtil.hasLongDesc(node)) {
        return cvox.AbstractEarcons.LONG_DESC;
      }
      break;
    case 'LI':
      return cvox.AbstractEarcons.LIST_ITEM;
    case 'SELECT':
      return cvox.AbstractEarcons.LISTBOX;
    case 'TEXTAREA':
      return cvox.AbstractEarcons.EDITABLE_TEXT;
    case 'INPUT':
      switch (node.type) {
        case 'button':
        case 'submit':
        case 'reset':
          return cvox.AbstractEarcons.BUTTON;
        case 'checkbox':
        case 'radio':
          if (node.checked) {
            return cvox.AbstractEarcons.CHECK_ON;
          } else {
            return cvox.AbstractEarcons.CHECK_OFF;
          }
        default:
          if (cvox.DomUtil.isInputTypeText(node)) {
            // 'text', 'password', etc.
            return cvox.AbstractEarcons.EDITABLE_TEXT;
          }
      }
  }
  return null;
};
