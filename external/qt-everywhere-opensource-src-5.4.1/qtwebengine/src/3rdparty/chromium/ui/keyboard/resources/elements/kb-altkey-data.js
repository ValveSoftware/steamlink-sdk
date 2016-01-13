// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var altKeys = {};
  var idMap = {};

    /**
     * Creates a unique identifier based on the key provided.
     * This identifier is of the form 'idHASH' where HASH is
     * the concatenation of the keycodes of every character in the string.
     * @param {string} key Key for which we want an identifier.
     * @return {string} The unique id for key.
     */
     function createId(key) {
      var hash = key.split('').map(
        // Returns the key code for the character.
        function(character) {
          return character.charCodeAt(0);
        }
        ).join('');
      return 'id' + hash;
    }

    Polymer('kb-altkey-data', {

      /**
       * Retrieves a list of alternative keys to display on a long-press.
       * @param {string} char The base character.
       * @param {boolean=} opt_force If true, force the creation of a list
       *    even if empty. Used when constructing a set of alternates for keys
       *    with hintTexts.
       * @return {?Object.{id: string, list: string}}
       */
       getAltkeys: function(char, opt_force) {
        var id = idMap[char];
        if (id) {
          return {
            'id': id,
            'keys': altKeys[id]
          };
        }
        if (opt_force) {
          return {
            'id': createId(char),
            'keys': []
          };
        }
      },

      /**
       * Registers lists of alternative keys displayed on a long-press.
       * @param {Object.<string, Array.<string>>} data Mapping of characters to
       *     lists of alternatives.
       */
       registerAltkeys: function(data) {
        for (var key in data) {
          var id = idMap[key];
          if (!id)
            idMap[key] = id = createId(key);
          altKeys[id] = data[key];
        }
      },

      /**
       * Creates a list of alternate candidates to display in a popup on a
       * long-press.
       * @param {string} char The base character.
       * @param {number} maxLeftOffset Limits the number of candidates
       *      displayed to the left of the base character to prevent running
       *      past the left edge of the keyboard.
       * @param {number} maxRightOffset Limits the number of candidates
       *     displayed to the right of the base character to prvent running
       *     past the right edge of the keyboard.
       * @param {string=} opt_additionalKeys Optional list of additional keys
       *     to include in the candidates list.
       */
       createAltkeySet: function(char,
        maxLeftOffset,
        maxRightOffset,
        opt_additionalKeys) {
        var altKeys = this.getAltkeys(char, true /* forced */);
        if (altKeys) {
          var list = altKeys.keys;
          if (opt_additionalKeys)
            list = opt_additionalKeys.split('').concat(list);
          list = [char].concat(list);

          var set = document.createElement('kb-altkey-set');
          // Candiates are approximately in decreasing order of usage, and are
          // arranged in a single row in the popup display.  To reduce the
          // expected length of the drag gesture for selecting a candidate,
          // more likely candidates are placed in the center of the popup,
          // which is achieved by alternately appending and prepending
          // candiates in the alternatives popup.
          var prepend = false;
          var leftOffset = 0;
          var rightOffset = 0;
          for (var i = 0; i < list.length; i++) {
            var key = document.createElement('kb-altkey');
            key.textContent = list[i];
            if (prepend) {
              set.insertBefore(key, set.firstChild);
              leftOffset++;
            } else {
              set.appendChild(key);
              rightOffset++;
            }
            prepend = !prepend;
            // Verify that there is room remaining for an additional character.
            if (leftOffset == maxLeftOffset && rightOffset == maxRightOffset)
              break;
            if (leftOffset == maxLeftOffset)
              prepend = false;
            else if (rightOffset == maxRightOffset)
              prepend = true;
          }
          set.id = altKeys.id;
          set.offset = leftOffset;
          return set;
        }
      },

    });
})();
