// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A multiple gnubby signer wraps the process of opening a number
 * of gnubbies, signing each challenge in an array of challenges until a
 * success condition is satisfied, and yielding each succeeding gnubby.
 */
'use strict';

/**
 * Creates a new sign handler with an array of gnubby indexes.
 * @param {!GnubbyFactory} factory Used to create and open the gnubbies.
 * @param {Array.<llGnubbyDeviceId>} gnubbyIndexes Which gnubbies to open.
 * @param {boolean} forEnroll Whether this signer is signing for an attempted
 *     enroll operation.
 * @param {function(boolean, (number|undefined))} completedCb Called when this
 *     signer completes sign attempts, i.e. no further results should be
 *     expected.
 * @param {function(number, MultipleSignerResult)} gnubbyFoundCb Called with
 *     each gnubby/challenge that yields a successful result.
 * @param {Countdown=} opt_timer An advisory timer, beyond whose expiration the
 *     signer will not attempt any new operations, assuming the caller is no
 *     longer interested in the outcome.
 * @param {string=} opt_logMsgUrl A URL to post log messages to.
 * @constructor
 */
function MultipleGnubbySigner(factory, gnubbyIndexes, forEnroll, completedCb,
    gnubbyFoundCb, opt_timer, opt_logMsgUrl) {
  /** @private {!GnubbyFactory} */
  this.factory_ = factory;
  /** @private {Array.<llGnubbyDeviceId>} */
  this.gnubbyIndexes_ = gnubbyIndexes;
  /** @private {boolean} */
  this.forEnroll_ = forEnroll;
  /** @private {function(boolean, (number|undefined))} */
  this.completedCb_ = completedCb;
  /** @private {function(number, MultipleSignerResult)} */
  this.gnubbyFoundCb_ = gnubbyFoundCb;
  /** @private {Countdown|undefined} */
  this.timer_ = opt_timer;
  /** @private {string|undefined} */
  this.logMsgUrl_ = opt_logMsgUrl;

  /** @private {Array.<SignHelperChallenge>} */
  this.challenges_ = [];
  /** @private {boolean} */
  this.challengesFinal_ = false;

  // Create a signer for each gnubby.
  /** @private {boolean} */
  this.anySucceeded_ = false;
  /** @private {number} */
  this.numComplete_ = 0;
  /** @private {Array.<SingleGnubbySigner>} */
  this.signers_ = [];
  /** @private {Array.<boolean>} */
  this.stillGoing_ = [];
  /** @private {Array.<number>} */
  this.errorStatus_ = [];
  for (var i = 0; i < gnubbyIndexes.length; i++) {
    this.addGnubby(gnubbyIndexes[i]);
  }
}

/**
 * Attempts to open this signer's gnubbies, if they're not already open.
 * (This is implicitly done by addChallenges.)
 */
MultipleGnubbySigner.prototype.open = function() {
  for (var i = 0; i < this.signers_.length; i++) {
    this.signers_[i].open();
  }
};

/**
 * Closes this signer's gnubbies, if any are open.
 */
MultipleGnubbySigner.prototype.close = function() {
  for (var i = 0; i < this.signers_.length; i++) {
    this.signers_[i].close();
  }
};

/**
 * Adds challenges to the set of challenges being tried by this signer.
 * The challenges are an array of challenge objects, where each challenge
 * object's values are base64-encoded.
 * If the signer is currently idle, begins signing the new challenges.
 *
 * @param {Array} challenges Encoded challenges
 * @param {boolean} finalChallenges True iff there are no more challenges to add
 * @return {boolean} whether the challenges were successfully added.
 */
MultipleGnubbySigner.prototype.addEncodedChallenges =
    function(challenges, finalChallenges) {
  var decodedChallenges = [];
  if (challenges) {
    for (var i = 0; i < challenges.length; i++) {
      var decodedChallenge = {};
      var challenge = challenges[i];
      decodedChallenge['challengeHash'] =
          B64_decode(challenge['challengeHash']);
      decodedChallenge['appIdHash'] = B64_decode(challenge['appIdHash']);
      decodedChallenge['keyHandle'] = B64_decode(challenge['keyHandle']);
      if (challenge['version']) {
        decodedChallenge['version'] = challenge['version'];
      }
      decodedChallenges.push(decodedChallenge);
    }
  }
  return this.addChallenges(decodedChallenges, finalChallenges);
};

/**
 * Adds challenges to the set of challenges being tried by this signer.
 * If the signer is currently idle, begins signing the new challenges.
 *
 * @param {Array.<SignHelperChallenge>} challenges Challenges to add
 * @param {boolean} finalChallenges True iff there are no more challnges to add
 * @return {boolean} whether the challenges were successfully added.
 */
MultipleGnubbySigner.prototype.addChallenges =
    function(challenges, finalChallenges) {
  if (this.challengesFinal_) {
    // Can't add new challenges once they're finalized.
    return false;
  }

  if (challenges) {
    for (var i = 0; i < challenges.length; i++) {
      this.challenges_.push(challenges[i]);
    }
  }
  this.challengesFinal_ = finalChallenges;

  for (var i = 0; i < this.signers_.length; i++) {
    this.stillGoing_[i] =
        this.signers_[i].addChallenges(challenges, finalChallenges);
    this.errorStatus_[i] = 0;
  }
  return true;
};

/**
 * Adds a new gnubby to this signer's list of gnubbies. (Only possible while
 * this signer is still signing: without this restriction, the morePossible
 * indication in the callbacks could become violated.) If this signer has
 * challenges to sign, begins signing on the new gnubby with them.
 * @param {llGnubbyDeviceId} gnubbyIndex The index of the gnubby to add.
 * @return {boolean} Whether the gnubby was added successfully.
 */
MultipleGnubbySigner.prototype.addGnubby = function(gnubbyIndex) {
  if (this.numComplete_ && this.numComplete_ == this.signers_.length)
    return false;

  var index = this.signers_.length;
  this.signers_.push(
      new SingleGnubbySigner(
          this.factory_,
          gnubbyIndex,
          this.forEnroll_,
          this.signFailedCallback_.bind(this, index),
          this.signSucceededCallback_.bind(this, index),
          this.timer_ ? this.timer_.clone() : null,
          this.logMsgUrl_));
  this.stillGoing_.push(false);

  if (this.challenges_.length) {
    this.stillGoing_[index] =
        this.signers_[index].addChallenges(this.challenges_,
            this.challengesFinal_);
  }
  return true;
};

/**
 * Called by a SingleGnubbySigner upon failure, i.e. unsuccessful completion of
 * all its sign operations.
 * @param {number} index the index of the gnubby whose result this is
 * @param {number} code the result code of the sign operation
 * @private
 */
MultipleGnubbySigner.prototype.signFailedCallback_ = function(index, code) {
  console.log(
      UTIL_fmt('failure. gnubby ' + index + ' got code ' + code.toString(16)));
  if (!this.stillGoing_[index]) {
    console.log(UTIL_fmt('gnubby ' + index + ' no longer running!'));
    // Shouldn't ever happen? Disregard.
    return;
  }
  this.stillGoing_[index] = false;
  this.errorStatus_[index] = code;
  this.numComplete_++;
  var morePossible = this.numComplete_ < this.signers_.length;
  if (!morePossible)
    this.notifyComplete_();
};

/**
 * Called by a SingleGnubbySigner upon success.
 * @param {number} index the index of the gnubby whose result this is
 * @param {usbGnubby} gnubby the underlying gnubby that succeded.
 * @param {number} code the result code of the sign operation
 * @param {SingleSignerResult=} signResult Result object
 * @private
 */
MultipleGnubbySigner.prototype.signSucceededCallback_ =
    function(index, gnubby, code, signResult) {
  console.log(UTIL_fmt('success! gnubby ' + index + ' got code ' +
      code.toString(16)));
  if (!this.stillGoing_[index]) {
    console.log(UTIL_fmt('gnubby ' + index + ' no longer running!'));
    // Shouldn't ever happen? Disregard.
    return;
  }
  this.anySucceeded_ = true;
  this.stillGoing_[index] = false;
  this.notifySuccess_(code, gnubby, index, signResult);
  this.numComplete_++;
  var morePossible = this.numComplete_ < this.signers_.length;
  if (!morePossible)
    this.notifyComplete_();
};

/**
 * @private
 */
MultipleGnubbySigner.prototype.notifyComplete_ = function() {
  // See if any of the signers failed with a strange error. If so, report a
  // single error to the caller, partly as a diagnostic aid and partly to
  // distinguish real failures from wrong data.
  var funnyBusiness;
  for (var i = 0; i < this.errorStatus_.length; i++) {
    if (this.errorStatus_[i] &&
        this.errorStatus_[i] != DeviceStatusCodes.WRONG_DATA_STATUS &&
        this.errorStatus_[i] != DeviceStatusCodes.WAIT_TOUCH_STATUS) {
      funnyBusiness = this.errorStatus_[i];
      break;
    }
  }
  if (funnyBusiness) {
    console.warn(UTIL_fmt('all done (success: ' + this.anySucceeded_ + ', ' +
        'funny error = ' + funnyBusiness + ')'));
  } else {
    console.log(UTIL_fmt('all done (success: ' + this.anySucceeded_ + ')'));
  }
  this.completedCb_(this.anySucceeded_, funnyBusiness);
};

/**
 * @param {number} code Success status code
 * @param {usbGnubby} gnubby The gnubby that succeeded
 * @param {number} gnubbyIndex The gnubby's index
 * @param {SingleSignerResult=} singleSignerResult Result object
 * @private
 */
MultipleGnubbySigner.prototype.notifySuccess_ =
    function(code, gnubby, gnubbyIndex, singleSignerResult) {
  console.log(UTIL_fmt('success (' + code.toString(16) + ')'));
  var signResult = {
    'gnubby': gnubby,
    'gnubbyIndex': gnubbyIndex
  };
  if (singleSignerResult && singleSignerResult['challenge'])
    signResult['challenge'] = singleSignerResult['challenge'];
  if (singleSignerResult && singleSignerResult['info'])
    signResult['info'] = singleSignerResult['info'];
  this.gnubbyFoundCb_(code, signResult);
};
