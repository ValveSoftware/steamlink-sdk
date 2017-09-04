// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /**
   * SupervisedUserListData class.
   * Handles requests for retrieving a list of existing supervised users which
   * are supervised by the current profile. For each request a promise is
   * returned, which is cached in order to reuse the retrieved supervised users
   * for future requests. The first request will be handled asynchronously.
   * @constructor
   * @class
   */
  function SupervisedUserListData() {
    this.observers_ = [];
  };

  cr.addSingletonGetter(SupervisedUserListData);

  /**
   * Receives a list of supervised users and resolves the promise.
   * @param {Array<Object>} supervisedUsers Array of supervised user objects.
   *     Each object is of the form:
   *       supervisedUser = {
   *         id: "Supervised User ID",
   *         name: "Supervised User Name",
   *         iconURL: "chrome://path/to/icon/image",
   *         onCurrentDevice: true or false,
   *         needAvatar: true or false
   *       }
   * @private
   */
  SupervisedUserListData.prototype.receiveExistingSupervisedUsers_ =
      function(supervisedUsers) {
    if (!this.promise_) {
      this.onDataChanged_(supervisedUsers);
      return;
    }
    this.resolve_(supervisedUsers);
  };

  /**
   * Called when there is a signin error when retrieving the list of supervised
   * users. Rejects the promise and resets the cached promise to null.
   * @private
   */
  SupervisedUserListData.prototype.onSigninError_ = function() {
    if (!this.promise_) {
      return;
    }
    this.reject_();
    this.resetPromise_();
  };

  /**
   * Handles the request for the list of existing supervised users by returning
   * a promise for the requested data. If there is no cached promise yet, a new
   * one will be created.
   * @return {Promise} The promise containing the list of supervised users.
   * @private
   */
  SupervisedUserListData.prototype.requestExistingSupervisedUsers_ =
      function() {
    if (this.promise_)
      return this.promise_;
    this.promise_ = this.createPromise_();
    chrome.send('requestSupervisedUserImportUpdate');
    return this.promise_;
  };

  /**
   * Creates the promise containing the list of supervised users. The promise is
   * resolved in receiveExistingSupervisedUsers_() or rejected in
   * onSigninError_(). The promise is cached, so that for future requests it can
   * be resolved immediately.
   * @return {Promise} The promise containing the list of supervised users.
   * @private
   */
  SupervisedUserListData.prototype.createPromise_ = function() {
    var self = this;
    return new Promise(function(resolve, reject) {
      self.resolve_ = resolve;
      self.reject_ = reject;
    });
  };

  /**
   * Resets the promise to null in order to avoid stale data. For the next
   * request, a new promise will be created.
   * @private
   */
  SupervisedUserListData.prototype.resetPromise_ = function() {
    this.promise_ = null;
  };

  /**
   * Initializes |promise| with the new data and also passes the new data to
   * observers.
   * @param {Array<Object>} supervisedUsers Array of supervised user objects.
   *     For the format of the objects, see receiveExistingSupervisedUsers_().
   * @private
   */
  SupervisedUserListData.prototype.onDataChanged_ = function(supervisedUsers) {
    this.promise_ = this.createPromise_();
    this.resolve_(supervisedUsers);
    for (var i = 0; i < this.observers_.length; ++i)
      this.observers_[i].receiveExistingSupervisedUsers_(supervisedUsers);
  };

  /**
   * Adds an observer to the list of observers.
   * @param {Object} observer The observer to be added.
   * @private
   */
  SupervisedUserListData.prototype.addObserver_ = function(observer) {
    for (var i = 0; i < this.observers_.length; ++i)
      assert(this.observers_[i] != observer);
    this.observers_.push(observer);
  };

  /**
   * Removes an observer from the list of observers.
   * @param {Object} observer The observer to be removed.
   * @private
   */
  SupervisedUserListData.prototype.removeObserver_ = function(observer) {
    for (var i = 0; i < this.observers_.length; ++i) {
      if (this.observers_[i] == observer) {
        this.observers_.splice(i, 1);
        return;
      }
    }
  };

  // Forward public APIs to private implementations.
  cr.makePublic(SupervisedUserListData, [
    'addObserver',
    'onSigninError',
    'receiveExistingSupervisedUsers',
    'removeObserver',
    'requestExistingSupervisedUsers',
    'resetPromise',
  ]);

  // Export
  return {
    SupervisedUserListData: SupervisedUserListData,
  };
});
