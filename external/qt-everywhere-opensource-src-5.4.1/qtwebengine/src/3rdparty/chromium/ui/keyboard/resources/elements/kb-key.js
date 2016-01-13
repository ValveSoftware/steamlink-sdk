// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer('kb-key', {
  /**
   * The background image to display on this key. Does not display an
   * image if this is the empty string.
   * @type {string}
   */
  image: "",

  /**
   * The background image size to use if an image is specified. The size
   * is provided as a string, for example, "50%".
   * @type {string}
   */
  imageSize: "",

  /**
   * Key codes have been deprecated in DOM3 key events, but are required
   * for legacy web content. The key codes depend on the position of the
   * key on the keyboard and is independent of which modifier keys (shift,
   *  alt, ...) are active.
   * @type {number|undefined}
   */
  keyCode: undefined,

  /**
   * Name of the key as defined in the DOM3 specification for key events.
   * Like the keyCode, the keyName is independent of the state of the
   * modifier keys.
   * @type {string|undefined}
   */
  keyName: undefined,

  /**
   * Whether the shift key is pressed when producing the key value.
   * @type {boolean}
   */
  shiftModifier: false,

  /**
   * The sound to play when this key is pressed.
   * @type {Sound}
   */
  sound: Sound.DEFAULT,

  /**
   * Whether the key can be stretched to accomodate pixel rounding errors.
   */
  stretch: false,

  /**
   * Weighting to use for layout in order to properly size the key.
   * Keys with a high weighting are wider than normal keys.
   * @type {number}
   */
  weight: DEFAULT_KEY_WEIGHT,

  /**
   * Called when the image attribute changes. This is used to set the
   * background image of the key.
   * TODO(rsadam): Remove when polymer {{}} syntax regression is fixed.
   */
  imageChanged: function() {
    if (!this.image) {
      this.$.key.style.backgroundImage = "none";
    } else {
      // If no extension provided, default to svg.
      var image =
          this.image.split('.').length > 1 ? this.image : this.image + ".svg";
      this.$.key.style.backgroundImage =
          "url(images/" + image + ")";
    }
  },

  /**
   * Returns a subset of the key attributes.
   * @param {string} caller The id of the function that called
   *     populateDetails.
   * @return {Object} Mapping of attributes for the key element.
   */
  populateDetails: function(caller) {
    var details = this.super([caller]);
    details.keyCode = this.keyCode;
    details.keyName = this.keyName;
    details.shiftModifier = this.shiftModifier;
    details.sound = this.sound;
    return details;
  },
});
;

Polymer('kb-abc-key', {
  populateDetails: function(caller) {
    var detail = this.super([caller]);
    switch (caller) {
      case ('down'):
        detail.relegateToShift = true;
        break;
      default:
        break;
    }
    return detail;
  }
});
;

Polymer('kb-hide-keyboard-key', {
  up: function(event) {
    hideKeyboard();
  },
});
