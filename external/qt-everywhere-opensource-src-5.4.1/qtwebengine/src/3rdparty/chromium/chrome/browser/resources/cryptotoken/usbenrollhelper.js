// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Implements an enroll helper using USB gnubbies.
 */
'use strict';

/**
 * @param {!GnubbyFactory} factory A factory for Gnubby instances
 * @param {!Countdown} timer A timer for enroll timeout
 * @param {function(number, boolean)} errorCb Called when an enroll request
 *     fails with an error code and whether any gnubbies were found.
 * @param {function(string, string)} successCb Called with the result of a
 *     successful enroll request, along with the version of the gnubby that
 *     provided it.
 * @param {(function(number, boolean)|undefined)} opt_progressCb Called with
 *     progress updates to the enroll request.
 * @param {string=} opt_logMsgUrl A URL to post log messages to.
 * @constructor
 * @implements {EnrollHelper}
 */
function UsbEnrollHelper(factory, timer, errorCb, successCb, opt_progressCb,
    opt_logMsgUrl) {
  /** @private {!GnubbyFactory} */
  this.factory_ = factory;
  /** @private {!Countdown} */
  this.timer_ = timer;
  /** @private {function(number, boolean)} */
  this.errorCb_ = errorCb;
  /** @private {function(string, string)} */
  this.successCb_ = successCb;
  /** @private {(function(number, boolean)|undefined)} */
  this.progressCb_ = opt_progressCb;
  /** @private {string|undefined} */
  this.logMsgUrl_ = opt_logMsgUrl;

  /** @private {Array.<SignHelperChallenge>} */
  this.signChallenges_ = [];
  /** @private {boolean} */
  this.signChallengesFinal_ = false;
  /** @private {Array.<usbGnubby>} */
  this.waitingForTouchGnubbies_ = [];

  /** @private {boolean} */
  this.closed_ = false;
  /** @private {boolean} */
  this.notified_ = false;
  /** @private {number|undefined} */
  this.lastProgressUpdate_ = undefined;
  /** @private {boolean} */
  this.signerComplete_ = false;
  this.getSomeGnubbies_();
}

/**
 * Attempts to enroll using the provided data.
 * @param {Object} enrollChallenges a map of version string to enroll
 *     challenges.
 * @param {Array.<SignHelperChallenge>} signChallenges a list of sign
 *     challenges for already enrolled gnubbies, to prevent double-enrolling a
 *     device.
 */
UsbEnrollHelper.prototype.doEnroll =
    function(enrollChallenges, signChallenges) {
  this.enrollChallenges = enrollChallenges;
  this.signChallengesFinal_ = true;
  if (this.signer_) {
    this.signer_.addEncodedChallenges(
        signChallenges, this.signChallengesFinal_);
  } else {
    this.signChallenges_ = signChallenges;
  }
};

/** Closes this helper. */
UsbEnrollHelper.prototype.close = function() {
  this.closed_ = true;
  for (var i = 0; i < this.waitingForTouchGnubbies_.length; i++) {
    this.waitingForTouchGnubbies_[i].closeWhenIdle();
  }
  this.waitingForTouchGnubbies_ = [];
  if (this.signer_) {
    this.signer_.close();
    this.signer_ = null;
  }
};

/**
 * Enumerates gnubbies, and begins processing challenges upon enumeration if
 * any gnubbies are found.
 * @private
 */
UsbEnrollHelper.prototype.getSomeGnubbies_ = function() {
  this.factory_.enumerate(this.enumerateCallback_.bind(this));
};

/**
 * Called with the result of enumerating gnubbies.
 * @param {number} rc the result of the enumerate.
 * @param {Array.<llGnubbyDeviceId>} indexes Device ids of enumerated gnubbies
 * @private
 */
UsbEnrollHelper.prototype.enumerateCallback_ = function(rc, indexes) {
  if (rc) {
    // Enumerate failure is rare enough that it might be worth reporting
    // directly, rather than trying again.
    this.errorCb_(rc, false);
    return;
  }
  if (!indexes.length) {
    this.maybeReEnumerateGnubbies_();
    return;
  }
  if (this.timer_.expired()) {
    this.errorCb_(DeviceStatusCodes.TIMEOUT_STATUS, true);
    return;
  }
  this.gotSomeGnubbies_(indexes);
};

/**
 * If there's still time, re-enumerates devices and try with them. Otherwise
 * reports an error and, implicitly, stops the enroll operation.
 * @private
 */
UsbEnrollHelper.prototype.maybeReEnumerateGnubbies_ = function() {
  var errorCode = DeviceStatusCodes.WRONG_DATA_STATUS;
  var anyGnubbies = false;
  // If there's still time and we're still going, retry enumerating.
  if (!this.closed_ && !this.timer_.expired()) {
    this.notifyProgress_(errorCode, anyGnubbies);
    var self = this;
    // Use a delayed re-enumerate to prevent hammering the system unnecessarily.
    window.setTimeout(function() {
      if (self.timer_.expired()) {
        self.notifyError_(errorCode, anyGnubbies);
      } else {
        self.getSomeGnubbies_();
      }
    }, 200);
  } else {
    this.notifyError_(errorCode, anyGnubbies);
  }
};

/**
 * Called with the result of enumerating gnubby indexes.
 * @param {Array.<llGnubbyDeviceId>} indexes Device ids of enumerated gnubbies
 * @private
 */
UsbEnrollHelper.prototype.gotSomeGnubbies_ = function(indexes) {
  this.signer_ = new MultipleGnubbySigner(
      this.factory_,
      indexes,
      true /* forEnroll */,
      this.signerCompleted_.bind(this),
      this.signerFoundGnubby_.bind(this),
      this.timer_,
      this.logMsgUrl_);
  if (this.signChallengesFinal_) {
    this.signer_.addEncodedChallenges(
        this.signChallenges_, this.signChallengesFinal_);
    this.pendingSignChallenges_ = [];
  }
};

/**
 * Called when a MultipleGnubbySigner completes its sign request.
 * @param {boolean} anySucceeded whether any sign attempt completed
 *     successfully.
 * @param {number=} errorCode an error code from a failing gnubby, if one was
 *     found.
 * @private
 */
UsbEnrollHelper.prototype.signerCompleted_ = function(anySucceeded, errorCode) {
  this.signerComplete_ = true;
  // The signer is not created unless some gnubbies were enumerated, so
  // anyGnubbies is mostly always true. The exception is when the last gnubby is
  // removed, handled shortly.
  var anyGnubbies = true;
  if (!anySucceeded) {
    if (errorCode == -llGnubby.GONE) {
      // If the last gnubby was removed, report as though no gnubbies were
      // found.
      this.maybeReEnumerateGnubbies_();
    } else {
      if (!errorCode) errorCode = DeviceStatusCodes.WRONG_DATA_STATUS;
      this.notifyError_(errorCode, anyGnubbies);
    }
  } else if (this.anyTimeout) {
    // Some previously succeeding gnubby timed out: return its error code.
    this.notifyError_(this.timeoutError, anyGnubbies);
  } else {
    // Do nothing: signerFoundGnubby will have been called with each succeeding
    // gnubby.
  }
};

/**
 * Called when a MultipleGnubbySigner finds a gnubby that can enroll.
 * @param {number} code Status code
 * @param {MultipleSignerResult} signResult Signature results
 * @private
 */
UsbEnrollHelper.prototype.signerFoundGnubby_ = function(code, signResult) {
  var gnubby = signResult['gnubby'];
  this.waitingForTouchGnubbies_.push(gnubby);
  this.notifyProgress_(DeviceStatusCodes.WAIT_TOUCH_STATUS, true);
  if (code == DeviceStatusCodes.WRONG_DATA_STATUS) {
    if (signResult['challenge']) {
      // If the signer yielded a busy open, indicate waiting for touch
      // immediately, rather than attempting enroll. This allows the UI to
      // update, since a busy open is a potentially long operation.
      this.notifyError_(DeviceStatusCodes.WAIT_TOUCH_STATUS, true);
    } else {
      this.matchEnrollVersionToGnubby_(gnubby);
    }
  }
};

/**
 * Attempts to match the gnubby's U2F version with an appropriate enroll
 * challenge.
 * @param {usbGnubby} gnubby Gnubby instance
 * @private
 */
UsbEnrollHelper.prototype.matchEnrollVersionToGnubby_ = function(gnubby) {
  if (!gnubby) {
    console.warn(UTIL_fmt('no gnubby, WTF?'));
  }
  gnubby.version(this.gnubbyVersioned_.bind(this, gnubby));
};

/**
 * Called with the result of a version command.
 * @param {usbGnubby} gnubby Gnubby instance
 * @param {number} rc result of version command.
 * @param {ArrayBuffer=} data version.
 * @private
 */
UsbEnrollHelper.prototype.gnubbyVersioned_ = function(gnubby, rc, data) {
  if (rc) {
    this.removeWrongVersionGnubby_(gnubby);
    return;
  }
  var version = UTIL_BytesToString(new Uint8Array(data || null));
  this.tryEnroll_(gnubby, version);
};

/**
 * Drops the gnubby from the list of eligible gnubbies.
 * @param {usbGnubby} gnubby Gnubby instance
 * @private
 */
UsbEnrollHelper.prototype.removeWaitingGnubby_ = function(gnubby) {
  gnubby.closeWhenIdle();
  var index = this.waitingForTouchGnubbies_.indexOf(gnubby);
  if (index >= 0) {
    this.waitingForTouchGnubbies_.splice(index, 1);
  }
};

/**
 * Drops the gnubby from the list of eligible gnubbies, as it has the wrong
 * version.
 * @param {usbGnubby} gnubby Gnubby instance
 * @private
 */
UsbEnrollHelper.prototype.removeWrongVersionGnubby_ = function(gnubby) {
  this.removeWaitingGnubby_(gnubby);
  if (!this.waitingForTouchGnubbies_.length && this.signerComplete_) {
    // Whoops, this was the last gnubby: indicate there are none.
    this.notifyError_(DeviceStatusCodes.WRONG_DATA_STATUS, false);
  }
};

/**
 * Attempts enrolling a particular gnubby with a challenge of the appropriate
 * version.
 * @param {usbGnubby} gnubby Gnubby instance
 * @param {string} version Protocol version
 * @private
 */
UsbEnrollHelper.prototype.tryEnroll_ = function(gnubby, version) {
  var challenge = this.getChallengeOfVersion_(version);
  if (!challenge) {
    this.removeWrongVersionGnubby_(gnubby);
    return;
  }
  var challengeChallenge = B64_decode(challenge['challenge']);
  var appIdHash = B64_decode(challenge['appIdHash']);
  gnubby.enroll(challengeChallenge, appIdHash,
      this.enrollCallback_.bind(this, gnubby, version));
};

/**
 * Finds the (first) challenge of the given version in this helper's challenges.
 * @param {string} version Protocol version
 * @return {Object} challenge, if found, or null if not.
 * @private
 */
UsbEnrollHelper.prototype.getChallengeOfVersion_ = function(version) {
  for (var i = 0; i < this.enrollChallenges.length; i++) {
    if (this.enrollChallenges[i]['version'] == version) {
      return this.enrollChallenges[i];
    }
  }
  return null;
};

/**
 * Called with the result of an enroll request to a gnubby.
 * @param {usbGnubby} gnubby Gnubby instance
 * @param {string} version Protocol version
 * @param {number} code Status code
 * @param {ArrayBuffer=} infoArray Returned data
 * @private
 */
UsbEnrollHelper.prototype.enrollCallback_ =
    function(gnubby, version, code, infoArray) {
  if (this.notified_) {
    // Enroll completed after previous success or failure. Disregard.
    return;
  }
  switch (code) {
    case -llGnubby.GONE:
        // Close this gnubby.
        this.removeWaitingGnubby_(gnubby);
        if (!this.waitingForTouchGnubbies_.length) {
          // Last enroll attempt is complete and last gnubby is gone: retry if
          // possible.
          this.maybeReEnumerateGnubbies_();
        }
      break;

    case DeviceStatusCodes.WAIT_TOUCH_STATUS:
    case DeviceStatusCodes.BUSY_STATUS:
    case DeviceStatusCodes.TIMEOUT_STATUS:
      if (this.timer_.expired()) {
        // Store any timeout error code, to be returned from the complete
        // callback if no other eligible gnubbies are found.
        this.anyTimeout = true;
        this.timeoutError = code;
        // Close this gnubby.
        this.removeWaitingGnubby_(gnubby);
        if (!this.waitingForTouchGnubbies_.length && !this.notified_) {
          // Last enroll attempt is complete: return this error.
          console.log(UTIL_fmt('timeout (' + code.toString(16) +
              ') enrolling'));
          this.notifyError_(code, true);
        }
      } else {
        // Notify caller of waiting for touch events.
        if (code == DeviceStatusCodes.WAIT_TOUCH_STATUS) {
          this.notifyProgress_(code, true);
        }
        window.setTimeout(this.tryEnroll_.bind(this, gnubby, version), 200);
      }
      break;

    case DeviceStatusCodes.OK_STATUS:
      var info = B64_encode(new Uint8Array(infoArray || []));
      this.notifySuccess_(version, info);
      break;

    default:
      console.log(UTIL_fmt('Failed to enroll gnubby: ' + code));
      this.notifyError_(code, true);
      break;
  }
};

/**
 * @param {number} code Status code
 * @param {boolean} anyGnubbies If any gnubbies were found
 * @private
 */
UsbEnrollHelper.prototype.notifyError_ = function(code, anyGnubbies) {
  if (this.notified_ || this.closed_)
    return;
  this.notified_ = true;
  this.close();
  this.errorCb_(code, anyGnubbies);
};

/**
 * @param {string} version Protocol version
 * @param {string} info B64 encoded success data
 * @private
 */
UsbEnrollHelper.prototype.notifySuccess_ = function(version, info) {
  if (this.notified_ || this.closed_)
    return;
  this.notified_ = true;
  this.close();
  this.successCb_(version, info);
};

/**
 * @param {number} code Status code
 * @param {boolean} anyGnubbies If any gnubbies were found
 * @private
 */
UsbEnrollHelper.prototype.notifyProgress_ = function(code, anyGnubbies) {
  if (this.lastProgressUpdate_ == code || this.notified_ || this.closed_)
    return;
  this.lastProgressUpdate_ = code;
  if (this.progressCb_) this.progressCb_(code, anyGnubbies);
};

/**
 * @param {!GnubbyFactory} gnubbyFactory factory to create gnubbies.
 * @constructor
 * @implements {EnrollHelperFactory}
 */
function UsbEnrollHelperFactory(gnubbyFactory) {
  /** @private {!GnubbyFactory} */
  this.gnubbyFactory_ = gnubbyFactory;
}

/**
 * @param {!Countdown} timer Timeout timer
 * @param {function(number, boolean)} errorCb Called when an enroll request
 *     fails with an error code and whether any gnubbies were found.
 * @param {function(string, string)} successCb Called with the result of a
 *     successful enroll request, along with the version of the gnubby that
 *     provided it.
 * @param {(function(number, boolean)|undefined)} opt_progressCb Called with
 *     progress updates to the enroll request.
 * @param {string=} opt_logMsgUrl A URL to post log messages to.
 * @return {UsbEnrollHelper} the newly created helper.
 */
UsbEnrollHelperFactory.prototype.createHelper =
    function(timer, errorCb, successCb, opt_progressCb, opt_logMsgUrl) {
  var helper =
      new UsbEnrollHelper(this.gnubbyFactory_, timer, errorCb, successCb,
          opt_progressCb, opt_logMsgUrl);
  return helper;
};
