// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Implements a sign helper using USB gnubbies.
 */
'use strict';

var CORRUPT_sign = false;

/**
 * @param {!GnubbyFactory} factory Factory for gnubby instances
 * @param {Countdown} timer Timer after whose expiration the caller is no longer
 *     interested in the result of a sign request.
 * @param {function(number, boolean)} errorCb Called when a sign request fails
 *     with an error code and whether any gnubbies were found.
 * @param {function(SignHelperChallenge, string, string=)} successCb Called with
 *     the signature produced by a successful sign request.
 * @param {string=} opt_logMsgUrl A URL to post log messages to.
 * @constructor
 * @implements {SignHelper}
 */
function UsbSignHelper(factory, timer, errorCb, successCb, opt_logMsgUrl) {
  /** @private {!GnubbyFactory} */
  this.factory_ = factory;
  /** @private {Countdown} */
  this.timer_ = timer;
  /** @private {function(number, boolean)} */
  this.errorCb_ = errorCb;
  /** @private {function(SignHelperChallenge, string, string=)} */
  this.successCb_ = successCb;
  /** @private {string|undefined} */
  this.logMsgUrl_ = opt_logMsgUrl;

  /** @private {Array.<SignHelperChallenge>} */
  this.pendingChallenges_ = [];
  /** @private {Array.<usbGnubby>} */
  this.waitingForTouchGnubbies_ = [];

  /** @private {boolean} */
  this.notified_ = false;
  /** @private {boolean} */
  this.signerComplete_ = false;
}

/**
 * Attempts to sign the provided challenges.
 * @param {Array.<SignHelperChallenge>} challenges Challenges to sign
 * @return {boolean} whether this set of challenges was accepted.
 */
UsbSignHelper.prototype.doSign = function(challenges) {
  if (!challenges.length) {
    // Fail a sign request with an empty set of challenges, and pretend to have
    // alerted the caller in case the enumerate is still pending.
    this.notified_ = true;
    return false;
  } else {
    this.pendingChallenges_ = challenges;
    this.getSomeGnubbies_();
    return true;
  }
};

/**
 * Enumerates gnubbies, and begins processing challenges upon enumeration if
 * any gnubbies are found.
 * @private
 */
UsbSignHelper.prototype.getSomeGnubbies_ = function() {
  this.factory_.enumerate(this.enumerateCallback.bind(this));
};

/**
 * Called with the result of enumerating gnubbies.
 * @param {number} rc the result of the enumerate.
 * @param {Array.<llGnubbyDeviceId>} indexes Indexes of found gnubbies
 */
UsbSignHelper.prototype.enumerateCallback = function(rc, indexes) {
  if (rc) {
    this.notifyError_(rc, false);
    return;
  }
  if (!indexes.length) {
    this.notifyError_(DeviceStatusCodes.WRONG_DATA_STATUS, false);
    return;
  }
  if (this.timer_.expired()) {
    this.notifyError_(DeviceStatusCodes.TIMEOUT_STATUS, true);
    return;
  }
  this.gotSomeGnubbies_(indexes);
};

/**
 * Called with the result of enumerating gnubby indexes.
 * @param {Array.<llGnubbyDeviceId>} indexes Indexes of found gnubbies
 * @private
 */
UsbSignHelper.prototype.gotSomeGnubbies_ = function(indexes) {
  /** @private {MultipleGnubbySigner} */
  this.signer_ = new MultipleGnubbySigner(
      this.factory_,
      indexes,
      false /* forEnroll */,
      this.signerCompleted_.bind(this),
      this.signerFoundGnubby_.bind(this),
      this.timer_,
      this.logMsgUrl_);
  this.signer_.addEncodedChallenges(this.pendingChallenges_, true);
};

/**
 * Called when a MultipleGnubbySigner completes its sign request.
 * @param {boolean} anySucceeded whether any sign attempt completed
 *     successfully.
 * @param {number=} errorCode an error code from a failing gnubby, if one was
 *     found.
 * @private
 */
UsbSignHelper.prototype.signerCompleted_ = function(anySucceeded, errorCode) {
  this.signerComplete_ = true;
  // The signer is not created unless some gnubbies were enumerated, so
  // anyGnubbies is mostly always true. The exception is when the last gnubby is
  // removed, handled shortly.
  var anyGnubbies = true;
  if (!anySucceeded) {
    if (!errorCode) {
      errorCode = DeviceStatusCodes.WRONG_DATA_STATUS;
    } else if (errorCode == -llGnubby.GONE) {
      // If the last gnubby was removed, report as though no gnubbies were
      // found.
      errorCode = DeviceStatusCodes.WRONG_DATA_STATUS;
      anyGnubbies = false;
    }
    this.notifyError_(errorCode, anyGnubbies);
  } else if (this.anyTimeout_) {
    // Some previously succeeding gnubby timed out: return its error code.
    this.notifyError_(this.timeoutError_, anyGnubbies);
  } else {
    // Do nothing: signerFoundGnubby_ will have been called with each
    // succeeding gnubby.
  }
};

/**
 * Called when a MultipleGnubbySigner finds a gnubby that has successfully
 * signed, or can successfully sign, one of the challenges.
 * @param {number} code Status code
 * @param {MultipleSignerResult} signResult Signer result object
 * @private
 */
UsbSignHelper.prototype.signerFoundGnubby_ = function(code, signResult) {
  var gnubby = signResult['gnubby'];
  var challenge = signResult['challenge'];
  var info = new Uint8Array(signResult['info']);
  if (code == DeviceStatusCodes.OK_STATUS && info.length > 0 && info[0]) {
    this.notifySuccess_(gnubby, challenge, info);
  } else {
    this.waitingForTouchGnubbies_.push(gnubby);
    this.retrySignIfNotTimedOut_(gnubby, challenge, code);
  }
};

/**
 * Reports the result of a successful sign operation.
 * @param {usbGnubby} gnubby Gnubby instance
 * @param {SignHelperChallenge} challenge Challenge signed
 * @param {Uint8Array} info Result data
 * @private
 */
UsbSignHelper.prototype.notifySuccess_ = function(gnubby, challenge, info) {
  if (this.notified_)
    return;
  this.notified_ = true;

  gnubby.closeWhenIdle();
  this.close();

  if (CORRUPT_sign) {
    CORRUPT_sign = false;
    info[info.length - 1] = info[info.length - 1] ^ 0xff;
  }
  var encodedChallenge = {};
  encodedChallenge['challengeHash'] = B64_encode(challenge['challengeHash']);
  encodedChallenge['appIdHash'] = B64_encode(challenge['appIdHash']);
  encodedChallenge['keyHandle'] = B64_encode(challenge['keyHandle']);
  this.successCb_(
      /** @type {SignHelperChallenge} */ (encodedChallenge), B64_encode(info),
      'USB');
};

/**
 * Reports error to the caller.
 * @param {number} code error to report
 * @param {boolean} anyGnubbies If any gnubbies were found
 * @private
 */
UsbSignHelper.prototype.notifyError_ = function(code, anyGnubbies) {
  if (this.notified_)
    return;
  this.notified_ = true;
  this.close();
  this.errorCb_(code, anyGnubbies);
};

/**
 * Retries signing a particular challenge on a gnubby.
 * @param {usbGnubby} gnubby Gnubby instance
 * @param {SignHelperChallenge} challenge Challenge to retry
 * @private
 */
UsbSignHelper.prototype.retrySign_ = function(gnubby, challenge) {
  var challengeHash = challenge['challengeHash'];
  var appIdHash = challenge['appIdHash'];
  var keyHandle = challenge['keyHandle'];
  gnubby.sign(challengeHash, appIdHash, keyHandle,
      this.signCallback_.bind(this, gnubby, challenge));
};

/**
 * Called when a gnubby completes a sign request.
 * @param {usbGnubby} gnubby Gnubby instance
 * @param {SignHelperChallenge} challenge Challenge to retry
 * @param {number} code Previous status code
 * @private
 */
UsbSignHelper.prototype.retrySignIfNotTimedOut_ =
    function(gnubby, challenge, code) {
  if (this.timer_.expired()) {
    // Store any timeout error code, to be returned from the complete
    // callback if no other eligible gnubbies are found.
    /** @private {boolean} */
    this.anyTimeout_ = true;
    /** @private {number} */
    this.timeoutError_ = code;
    this.removePreviouslyEligibleGnubby_(gnubby, code);
  } else {
    window.setTimeout(this.retrySign_.bind(this, gnubby, challenge), 200);
  }
};

/**
 * Removes a gnubby that was waiting for touch from the list, with the given
 * error code. If this is the last gnubby, notifies the caller of the error.
 * @param {usbGnubby} gnubby Gnubby instance
 * @param {number} code Previous status code
 * @private
 */
UsbSignHelper.prototype.removePreviouslyEligibleGnubby_ =
    function(gnubby, code) {
  // Close this gnubby.
  gnubby.closeWhenIdle();
  var index = this.waitingForTouchGnubbies_.indexOf(gnubby);
  if (index >= 0) {
    this.waitingForTouchGnubbies_.splice(index, 1);
  }
  if (!this.waitingForTouchGnubbies_.length && this.signerComplete_ &&
      !this.notified_) {
    // Last sign attempt is complete: return this error.
    console.log(UTIL_fmt('timeout or error (' + code.toString(16) +
        ') signing'));
    // If the last device is gone, report as if no gnubbies were found.
    if (code == -llGnubby.GONE) {
      this.notifyError_(DeviceStatusCodes.WRONG_DATA_STATUS, false);
      return;
    }
    this.notifyError_(code, true);
  }
};

/**
 * Called when a gnubby completes a sign request.
 * @param {usbGnubby} gnubby Gnubby instance
 * @param {SignHelperChallenge} challenge Challenge signed
 * @param {number} code Status code
 * @param {ArrayBuffer=} infoArray Result data
 * @private
 */
UsbSignHelper.prototype.signCallback_ =
    function(gnubby, challenge, code, infoArray) {
  if (this.notified_) {
    // Individual sign completed after previous success or failure. Disregard.
    return;
  }
  var info = new Uint8Array(infoArray || []);
  if (code == DeviceStatusCodes.OK_STATUS && info.length > 0 && info[0]) {
    this.notifySuccess_(gnubby, challenge, info);
  } else if (code == DeviceStatusCodes.OK_STATUS ||
      code == DeviceStatusCodes.WAIT_TOUCH_STATUS ||
      code == DeviceStatusCodes.BUSY_STATUS) {
    this.retrySignIfNotTimedOut_(gnubby, challenge, code);
  } else {
    console.log(UTIL_fmt('got error ' + code.toString(16) + ' signing'));
    this.removePreviouslyEligibleGnubby_(gnubby, code);
  }
};

/**
 * Closes the MultipleGnubbySigner, if any.
 */
UsbSignHelper.prototype.close = function() {
  if (this.signer_) {
    this.signer_.close();
    this.signer_ = null;
  }
  for (var i = 0; i < this.waitingForTouchGnubbies_.length; i++) {
    this.waitingForTouchGnubbies_[i].closeWhenIdle();
  }
  this.waitingForTouchGnubbies_ = [];
};

/**
 * @param {!GnubbyFactory} gnubbyFactory Factory to create gnubbies.
 * @constructor
 * @implements {SignHelperFactory}
 */
function UsbSignHelperFactory(gnubbyFactory) {
  /** @private {!GnubbyFactory} */
  this.gnubbyFactory_ = gnubbyFactory;
}

/**
 * @param {Countdown} timer Timer after whose expiration the caller is no longer
 *     interested in the result of a sign request.
 * @param {function(number, boolean)} errorCb Called when a sign request fails
 *     with an error code and whether any gnubbies were found.
 * @param {function(SignHelperChallenge, string)} successCb Called with the
 *     signature produced by a successful sign request.
 * @param {string=} opt_logMsgUrl A URL to post log messages to.
 * @return {UsbSignHelper} the newly created helper.
 */
UsbSignHelperFactory.prototype.createHelper =
    function(timer, errorCb, successCb, opt_logMsgUrl) {
  var helper =
      new UsbSignHelper(this.gnubbyFactory_, timer, errorCb, successCb,
          opt_logMsgUrl);
  return helper;
};
