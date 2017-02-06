// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-users-add-user-dialog' is the dialog shown for adding new allowed
 * users to a ChromeOS device.
 */
(function() {

/**
 * Regular expression for adding a user where the string provided is just
 * the part before the "@".
 * Email alias only, assuming it's a gmail address.
 *     e.g. 'john'
 * @const {!RegExp}
 */
var NAME_ONLY_REGEX = new RegExp(
    '^\\s*([\\w\\.!#\\$%&\'\\*\\+-\\/=\\?\\^`\\{\\|\\}~]+)\\s*$');

/**
 * Regular expression for adding a user where the string provided is a full
 * email address.
 *     e.g. 'john@chromium.org'
 * @const {!RegExp}
 */
var EMAIL_REGEX = new RegExp(
    '^\\s*([\\w\\.!#\\$%&\'\\*\\+-\\/=\\?\\^`\\{\\|\\}~]+)@' +
    '([A-Za-z0-9\-]{2,63}\\..+)\\s*$');

Polymer({
  is: 'settings-users-add-user-dialog',

  open: function() {
    this.$.dialog.open();
  },

  /** @private */
  onCancelTap_: function() {
    this.$.dialog.cancel();
  },

  /**
   * Validates that the new user entered is valid.
   * @private
   * @return {boolean}
   */
  validate_: function() {
    var input = this.$.addUserInput.value;
    var valid = NAME_ONLY_REGEX.test(input) || EMAIL_REGEX.test(input);

    this.$.add.disabled = !valid;
    this.$.addUserInput.invalid = !valid;
    return valid;
  },

  /** @private */
  addUser_: function() {
    assert(this.validate_());

    var input = this.$.addUserInput.value;

    var nameOnlyMatches = NAME_ONLY_REGEX.exec(input);
    var userEmail;
    if (nameOnlyMatches) {
      userEmail = nameOnlyMatches[1] + '@gmail.com';
    } else {
      var emailMatches = EMAIL_REGEX.exec(input);
      // Assuming the input validated, one of these two must match.
      assert(emailMatches);
      userEmail = emailMatches[1] + '@' + emailMatches[2];
    }

    chrome.usersPrivate.addWhitelistedUser(
        userEmail,
        /* callback */ function(success) {});
    this.$.addUserInput.value = '';
    this.$.dialog.close();
  },
});

})();
