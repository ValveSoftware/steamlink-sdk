// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-expand-button' is a chrome-specific wrapper around paper-icon-button that
 * toggles between an opened (expanded) and closed state.
 *
 * Example:
 *
 *    <cr-expand-button expanded="{{sectionIsExpanded}}"></cr-expand-button>
 */
Polymer({
  is: 'cr-expand-button',

  properties: {
    /**
     * If true, the button is in the expanded state and will show the
     * 'expand-less' icon. If false, the button shows the 'expand-more' icon.
     */
    expanded: {
      type: Boolean,
      value: false,
      notify: true
    },

    /**
     * If true, the button will be disabled and greyed out.
     */
    disabled: {
      type: Boolean,
      value: false,
      reflectToAttribute: true
    },
  },

  iconName_: function(expanded) {
    return expanded ? 'cr:expand-less' : 'cr:expand-more';
  }
});
