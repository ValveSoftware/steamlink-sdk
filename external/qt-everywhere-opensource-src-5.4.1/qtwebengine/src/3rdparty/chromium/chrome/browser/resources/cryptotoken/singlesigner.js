// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A single gnubby signer wraps the process of opening a gnubby,
 * signing each challenge in an array of challenges until a success condition
 * is satisfied, and finally yielding the gnubby upon success.
 */

'use strict';

/**
 * Creates a new sign handler with a gnubby. This handler will perform a sign
 * operation using each challenge in an array of challenges until its success
 * condition is satisified, or an error or timeout occurs. The success condition
 * is defined differently depending whether this signer is used for enrolling
 * or for signing:
 *
 * For enroll, success is defined as each challenge yielding wrong data. This
 * means this gnubby is not currently enrolled for any of the appIds in any
 * challenge.
 *
 * For sign, success is defined as any challenge yielding ok or waiting for
 * touch.
 *
 * At most one of the success or failure callbacks will be called, and it will
 * be called at most once. Neither callback is guaranteed to be called: if
 * a final set of challenges is never given to this gnubby, or if the gnubby
 * stays busy, the signer has no way to know whether the set of challenges it's
 * been given has succeeded or failed.
 * The callback is called only when the signer reaches success or failure, i.e.
 * when there is no need for this signer to continue trying new challenges.
 *
 * @param {!GnubbyFactory} factory Used to create and open the gnubby.
 * @param {llGnubbyDeviceId} gnubbyIndex Which gnubby to open.
 * @param {boolean} forEnroll Whether this signer is signing for an attempted
 *     enroll operation.
 * @param {function(number)} errorCb Called when this signer fails, i.e. no
 *     further attempts can succeed.
 * @param {function(usbGnubby, number, (SingleSignerResult|undefined))}
 *     successCb Called when this signer succeeds.
 * @param {Countdown=} opt_timer An advisory timer, beyond whose expiration the
 *     signer will not attempt any new operations, assuming the caller is no
 *     longer interested in the outcome.
 * @param {string=} opt_logMsgUrl A URL to post log messages to.
 * @constructor
 */
function SingleGnubbySigner(factory, gnubbyIndex, forEnroll, errorCb, successCb,
    opt_timer, opt_logMsgUrl) {
  /** @private {GnubbyFactory} */
  this.factory_ = factory;
  /** @private {llGnubbyDeviceId} */
  this.gnubbyIndex_ = gnubbyIndex;
  /** @private {SingleGnubbySigner.State} */
  this.state_ = SingleGnubbySigner.State.INIT;
  /** @private {boolean} */
  this.forEnroll_ = forEnroll;
  /** @private {function(number)} */
  this.errorCb_ = errorCb;
  /** @private {function(usbGnubby, number, (SingleSignerResult|undefined))} */
  this.successCb_ = successCb;
  /** @private {Countdown|undefined} */
  this.timer_ = opt_timer;
  /** @private {string|undefined} */
  this.logMsgUrl_ = opt_logMsgUrl;

  /** @private {!Array.<!SignHelperChallenge>} */
  this.challenges_ = [];
  /** @private {number} */
  this.challengeIndex_ = 0;
  /** @private {boolean} */
  this.challengesFinal_ = false;

  /** @private {!Array.<string>} */
  this.notForMe_ = [];
}

/** @enum {number} */
SingleGnubbySigner.State = {
  /** Initial state. */
  INIT: 0,
  /** The signer is attempting to open a gnubby. */
  OPENING: 1,
  /** The signer's gnubby opened, but is busy. */
  BUSY: 2,
  /** The signer has an open gnubby, but no challenges to sign. */
  IDLE: 3,
  /** The signer is currently signing a challenge. */
  SIGNING: 4,
  /** The signer encountered an error. */
  ERROR: 5,
  /** The signer got a successful outcome. */
  SUCCESS: 6,
  /** The signer is closing its gnubby. */
  CLOSING: 7,
  /** The signer is closed. */
  CLOSED: 8
};

/**
 * Attempts to open this signer's gnubby, if it's not already open.
 * (This is implicitly done by addChallenges.)
 */
SingleGnubbySigner.prototype.open = function() {
  if (this.state_ == SingleGnubbySigner.State.INIT) {
    this.state_ = SingleGnubbySigner.State.OPENING;
    this.factory_.openGnubby(this.gnubbyIndex_,
                             this.forEnroll_,
                             this.openCallback_.bind(this),
                             this.logMsgUrl_);
  }
};

/**
 * Closes this signer's gnubby, if it's held.
 */
SingleGnubbySigner.prototype.close = function() {
  if (!this.gnubby_) return;
  this.state_ = SingleGnubbySigner.State.CLOSING;
  this.gnubby_.closeWhenIdle(this.closed_.bind(this));
};

/**
 * Called when this signer's gnubby is closed.
 * @private
 */
SingleGnubbySigner.prototype.closed_ = function() {
  this.gnubby_ = null;
  this.state_ = SingleGnubbySigner.State.CLOSED;
};

/**
 * Adds challenges to the set of challenges being tried by this signer.
 * If the signer is currently idle, begins signing the new challenges.
 *
 * @param {Array.<SignHelperChallenge>} challenges Sign challenges
 * @param {boolean} finalChallenges True if there are no more challenges to add
 * @return {boolean} Whether the challenges were accepted.
 */
SingleGnubbySigner.prototype.addChallenges =
    function(challenges, finalChallenges) {
  if (this.challengesFinal_) {
    // Can't add new challenges once they're finalized.
    return false;
  }

  if (challenges) {
    console.log(this.gnubby_);
    console.log(UTIL_fmt('adding ' + challenges.length + ' challenges'));
    for (var i = 0; i < challenges.length; i++) {
      this.challenges_.push(challenges[i]);
    }
  }
  this.challengesFinal_ = finalChallenges;

  switch (this.state_) {
    case SingleGnubbySigner.State.INIT:
      this.open();
      break;
    case SingleGnubbySigner.State.OPENING:
      // The open has already commenced, so accept the added challenges, but
      // don't do anything.
      break;
    case SingleGnubbySigner.State.IDLE:
      if (this.challengeIndex_ < challenges.length) {
        // New challenges added: restart signing.
        this.doSign_(this.challengeIndex_);
      } else if (finalChallenges) {
        // Finalized with no new challenges can happen when the caller rejects
        // the appId for some challenge.
        // If this signer is for enroll, the request must be rejected: this
        // signer can't determine whether the gnubby is or is not enrolled for
        // the origin.
        // If this signer is for sign, the request must also be rejected: there
        // are no new challenges to sign, and all previous ones did not yield
        // success.
        var self = this;
        window.setTimeout(function() {
          self.goToError_(DeviceStatusCodes.WRONG_DATA_STATUS);
        }, 0);
      }
      break;
    case SingleGnubbySigner.State.SIGNING:
      // Already signing, so don't kick off a new sign, but accept the added
      // challenges.
      break;
    default:
      return false;
  }
  return true;
};

/**
 * How long to delay retrying a failed open.
 */
SingleGnubbySigner.OPEN_DELAY_MILLIS = 200;

/**
 * @param {number} rc The result of the open operation.
 * @param {usbGnubby=} gnubby The opened gnubby, if open was successful (or
 *     busy).
 * @private
 */
SingleGnubbySigner.prototype.openCallback_ = function(rc, gnubby) {
  if (this.state_ != SingleGnubbySigner.State.OPENING &&
      this.state_ != SingleGnubbySigner.State.BUSY) {
    // Open completed after close, perhaps? Ignore.
    return;
  }

  switch (rc) {
    case DeviceStatusCodes.OK_STATUS:
      if (!gnubby) {
        console.warn(UTIL_fmt('open succeeded but gnubby is null, WTF?'));
      } else {
        this.gnubby_ = gnubby;
        this.gnubby_.version(this.versionCallback_.bind(this));
      }
      break;
    case DeviceStatusCodes.BUSY_STATUS:
      this.gnubby_ = gnubby;
      this.openedBusy_ = true;
      this.state_ = SingleGnubbySigner.State.BUSY;
      // If there's still time, retry the open.
      if (!this.timer_ || !this.timer_.expired()) {
        var self = this;
        window.setTimeout(function() {
          if (self.gnubby_) {
            self.factory_.openGnubby(self.gnubbyIndex_,
                                     self.forEnroll_,
                                     self.openCallback_.bind(self),
                                     self.logMsgUrl_);
          }
        }, SingleGnubbySigner.OPEN_DELAY_MILLIS);
      } else {
        this.goToError_(DeviceStatusCodes.BUSY_STATUS);
      }
      break;
    default:
      // TODO: This won't be confused with success, but should it be
      // part of the same namespace as the other error codes, which are
      // always in DeviceStatusCodes.*?
      this.goToError_(rc);
  }
};

/**
 * Called with the result of a version command.
 * @param {number} rc Result of version command.
 * @param {ArrayBuffer=} opt_data Version.
 * @private
 */
SingleGnubbySigner.prototype.versionCallback_ = function(rc, opt_data) {
  if (rc) {
    this.goToError_(rc);
    return;
  }
  this.state_ = SingleGnubbySigner.State.IDLE;
  this.version_ = UTIL_BytesToString(new Uint8Array(opt_data || []));
  this.doSign_(this.challengeIndex_);
};

/**
 * @param {number} challengeIndex Index of challenge to sign
 * @private
 */
SingleGnubbySigner.prototype.doSign_ = function(challengeIndex) {
  if (this.timer_ && this.timer_.expired()) {
    // If the timer is expired, that means we never got a success or a touch
    // required response: either always implies completion of this signer's
    // state machine (see signCallback's cases for OK_STATUS and
    // WAIT_TOUCH_STATUS.) We could have gotten wrong data on a partial set of
    // challenges, but this means we don't yet know the final outcome. In any
    // event, we don't yet know the final outcome: return busy.
    this.goToError_(DeviceStatusCodes.BUSY_STATUS);
    return;
  }

  this.state_ = SingleGnubbySigner.State.SIGNING;

  if (challengeIndex >= this.challenges_.length) {
    this.signCallback_(challengeIndex, DeviceStatusCodes.WRONG_DATA_STATUS);
    return;
  }

  var challenge = this.challenges_[challengeIndex];
  var challengeHash = challenge.challengeHash;
  var appIdHash = challenge.appIdHash;
  var keyHandle = challenge.keyHandle;
  if (this.notForMe_.indexOf(keyHandle) != -1) {
    // Cache hit: return wrong data again.
    this.signCallback_(challengeIndex, DeviceStatusCodes.WRONG_DATA_STATUS);
  } else if (challenge.version && challenge.version != this.version_) {
    // Sign challenge for a different version of gnubby: return wrong data.
    this.signCallback_(challengeIndex, DeviceStatusCodes.WRONG_DATA_STATUS);
  } else {
    var nowink = this.forEnroll_;
    this.gnubby_.sign(challengeHash, appIdHash, keyHandle,
        this.signCallback_.bind(this, challengeIndex),
        nowink);
  }
};

/**
 * Called with the result of a single sign operation.
 * @param {number} challengeIndex the index of the challenge just attempted
 * @param {number} code the result of the sign operation
 * @param {ArrayBuffer=} opt_info Optional result data
 * @private
 */
SingleGnubbySigner.prototype.signCallback_ =
    function(challengeIndex, code, opt_info) {
  console.log(UTIL_fmt('gnubby ' + JSON.stringify(this.gnubbyIndex_) +
      ', challenge ' + challengeIndex + ' yielded ' + code.toString(16)));
  if (this.state_ != SingleGnubbySigner.State.SIGNING) {
    console.log(UTIL_fmt('already done!'));
    // We're done, the caller's no longer interested.
    return;
  }

  // Cache wrong data result, re-asking the gnubby to sign it won't produce
  // different results.
  if (code == DeviceStatusCodes.WRONG_DATA_STATUS) {
    if (challengeIndex < this.challenges_.length) {
      var challenge = this.challenges_[challengeIndex];
      if (this.notForMe_.indexOf(challenge.keyHandle) == -1) {
        this.notForMe_.push(challenge.keyHandle);
      }
    }
  }

  switch (code) {
    case DeviceStatusCodes.GONE_STATUS:
      this.goToError_(code);
      break;

    case DeviceStatusCodes.TIMEOUT_STATUS:
      // TODO: On a TIMEOUT_STATUS, sync first, then retry.
    case DeviceStatusCodes.BUSY_STATUS:
      this.doSign_(this.challengeIndex_);
      break;

    case DeviceStatusCodes.OK_STATUS:
      if (this.forEnroll_) {
        this.goToError_(code);
      } else {
        this.goToSuccess_(code, this.challenges_[challengeIndex], opt_info);
      }
      break;

    case DeviceStatusCodes.WAIT_TOUCH_STATUS:
      if (this.forEnroll_) {
        this.goToError_(code);
      } else {
        this.goToSuccess_(code, this.challenges_[challengeIndex]);
      }
      break;

    case DeviceStatusCodes.WRONG_DATA_STATUS:
      if (this.challengeIndex_ < this.challenges_.length - 1) {
        this.doSign_(++this.challengeIndex_);
      } else if (!this.challengesFinal_) {
        this.state_ = SingleGnubbySigner.State.IDLE;
      } else if (this.forEnroll_) {
        // Signal the caller whether the open was busy, because it may take
        // an unusually long time when opened for enroll. Use an empty
        // "challenge" as the signal for a busy open.
        var challenge = undefined;
        if (this.openedBusy) {
          challenge = { appIdHash: '', challengeHash: '', keyHandle: '' };
        }
        this.goToSuccess_(code, challenge);
      } else {
        this.goToError_(code);
      }
      break;

    default:
      if (this.forEnroll_) {
        this.goToError_(code);
      } else if (this.challengeIndex_ < this.challenges_.length - 1) {
        this.doSign_(++this.challengeIndex_);
      } else if (!this.challengesFinal_) {
        // Increment the challenge index, as this one isn't useful any longer,
        // but a subsequent challenge may appear, and it might be useful.
        this.challengeIndex_++;
        this.state_ = SingleGnubbySigner.State.IDLE;
      } else {
        this.goToError_(code);
      }
  }
};

/**
 * Switches to the error state, and notifies caller.
 * @param {number} code Error code
 * @private
 */
SingleGnubbySigner.prototype.goToError_ = function(code) {
  this.state_ = SingleGnubbySigner.State.ERROR;
  console.log(UTIL_fmt('failed (' + code.toString(16) + ')'));
  this.errorCb_(code);
  // Since this gnubby can no longer produce a useful result, go ahead and
  // close it.
  this.close();
};

/**
 * Switches to the success state, and notifies caller.
 * @param {number} code Status code
 * @param {SignHelperChallenge=} opt_challenge The challenge signed
 * @param {ArrayBuffer=} opt_info Optional result data
 * @private
 */
SingleGnubbySigner.prototype.goToSuccess_ =
    function(code, opt_challenge, opt_info) {
  this.state_ = SingleGnubbySigner.State.SUCCESS;
  console.log(UTIL_fmt('success (' + code.toString(16) + ')'));
  if (opt_challenge || opt_info) {
    var singleSignerResult = {};
    if (opt_challenge) {
      singleSignerResult['challenge'] = opt_challenge;
    }
    if (opt_info) {
      singleSignerResult['info'] = opt_info;
    }
  }
  this.successCb_(this.gnubby_, code, singleSignerResult);
  // this.gnubby_ is now owned by successCb.
  this.gnubby_ = null;
};
