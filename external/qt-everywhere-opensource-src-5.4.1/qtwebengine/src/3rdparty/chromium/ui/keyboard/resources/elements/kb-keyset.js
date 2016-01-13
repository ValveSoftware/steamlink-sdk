// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer('kb-keyset', {
  align: "center",
  // Propagate flick gestures to keys in this keyset.
  flick: true,
  isDefault: false,
  nextKeyset: undefined,
  // Weight offsets for positioning the keyset.
  weightBottom: 15,
  weightLeft: 10,
  weightRight: 10,
  weightTop: 6,

  /**
   * Weight assigned to space between keys, of the form "xPitch yPitch".
   * Defaults to xPitch = yPitch if it's only a single number.
   * @type {string}
   */
  pitch: "10",

  /**
   * Expands kb-key-sequences into individual keys.
   */
  flattenKeyset: function() {
    var keySequences = this.querySelectorAll('kb-key-sequence');
    if (keySequences.length != 0) {
      keySequences.array().forEach(function(element) {
        var generatedDom = element.generateDom();
        element.parentNode.replaceChild(generatedDom, element);
      });
    }
  },

  // TODO(bshe): support select keyset on down, long and dbl events.
  keyUp: function(event, detail) {
    switch (detail.char) {
      case 'Shift':
      case 'Alt':
      case 'Ctrl':
      case 'Invalid':
        return;
      default:
        break;
    }
    if (!detail.toKeyset)
      detail.toKeyset = this.nextKeyset;
  },
  keyLongpress: function(event, detail) {
    if (!detail.char)
      return;

    var altkeyContainer = this.$.altkeyContainer.getDistributedNodes()[0];
    if (!altkeyContainer)
      return;

    var altkeyMetadata = this.$.altkeyMetadata;
    var altkeys = altkeyMetadata.getAltkeys(detail.char,
                                            !!detail.hintText);
    if (!altkeys)
      return;

    var id = altkeys.id;
    var activeAltKeySet = altkeyContainer.querySelector('#' + id);
    if (!activeAltKeySet) {
      var keyWidth = event.target.clientWidth;
      var leftMargin = event.target.offsetLeft;
      var maxLeftOffset = Math.round(leftMargin / keyWidth);
      var rightMargin = this.clientWidth - leftMargin - keyWidth;
      var maxRightOffset = Math.round(rightMargin / keyWidth);
      activeAltKeySet = altkeyMetadata.createAltkeySet(detail.char,
                                                       maxLeftOffset,
                                                       maxRightOffset,
                                                       detail.hintText);
      altkeyContainer.appendChild(activeAltKeySet);
    }

    altkeyContainer.keyset = id;
    event.target.dropKey();
    activeAltKeySet.style.width = event.target.clientWidth *
        activeAltKeySet.childElementCount + 'px';
    activeAltKeySet.style.height = event.target.clientHeight + 'px';
    activeAltKeySet.style.top = event.target.offsetTop + 'px';
    var leftOffset = activeAltKeySet.offset * event.target.clientWidth;
    activeAltKeySet.style.left = event.target.offsetLeft - leftOffset +
        'px';
    var nodes = activeAltKeySet.childNodes;
    nodes[activeAltKeySet.offset].classList.add('active');
    altkeyContainer.hidden = false;
  },

  show: function() {
    var old = $('keyboard').querySelector('.activeKeyset');
    if (old && old != this)
      old.classList.remove('activeKeyset');
    this.classList.add('activeKeyset');
    this.fire('stateChange', {
      state: 'keysetChanged',
      value: this.id
    });
  },
});
