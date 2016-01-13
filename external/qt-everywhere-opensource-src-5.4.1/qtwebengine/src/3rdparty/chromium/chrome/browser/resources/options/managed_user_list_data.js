// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /**
   * ManagedUserListData class.
   * Handles requests for retrieving a list of existing managed users which are
   * supervised by the current profile. For each request a promise is returned,
   * which is cached in order to reuse the retrieved managed users for future
   * requests. The first request will be handled asynchronously.
   * @constructor
   * @class
   */
  function ManagedUserListData() {
    this.observers_ = [];
  };

  cr.addSingletonGetter(ManagedUserListData);

  /**
   * Receives a list of managed users and resolves the promise.
   * @param {Array.<Object>} managedUsers An array of managed user objects.
   *     Each object is of the form:
   *       managedUser = {
   *         id: "Managed User ID",
   *         name: "Managed User Name",
   *         iconURL: "chrome://path/to/icon/image",
   *         onCurrentDevice: true or false,
   *         needAvatar: true or false
   *       }
   * @private
   */
  ManagedUserListData.prototype.receiveExistingManagedUsers_ = function(
      managedUsers) {
    if (!this.promise_) {
      this.onDataChanged_(managedUsers);
      return;
    }
    this.resolve_(managedUsers);
  };

  /**
   * Called when there is a signin error when retrieving the list of managed
   * users. Rejects the promise and resets the cached promise to null.
   * @private
   */
  ManagedUserListData.prototype.onSigninError_ = function() {
    assert(this.promise_);
    this.reject_();
    this.resetPromise_();
  };

  /**
   * Handles the request for the list of existing managed users by returning a
   * promise for the requested data. If there is no cached promise yet, a new
   * one will be created.
   * @return {Promise} The promise containing the list of managed users.
   * @private
   */
  ManagedUserListData.prototype.requestExistingManagedUsers_ = function() {
    if (this.promise_)
      return this.promise_;
    this.promise_ = this.createPromise_();
    chrome.send('requestManagedUserImportUpdate');
    return this.promise_;
  };

  /**
   * Creates the promise containing the list of managed users. The promise is
   * resolved in receiveExistingManagedUsers_() or rejected in
   * onSigninError_(). The promise is cached, so that for future requests it can
   * be resolved immediately.
   * @return {Promise} The promise containing the list of managed users.
   * @private
   */
  ManagedUserListData.prototype.createPromise_ = function() {
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
  ManagedUserListData.prototype.resetPromise_ = function() {
    this.promise_ = null;
  };

  /**
   * Initializes |promise| with the new data and also passes the new data to
   * observers.
   * @param {Array.<Object>} managedUsers An array of managed user objects.
   *     For the format of the objects, see receiveExistingManagedUsers_().
   * @private
   */
  ManagedUserListData.prototype.onDataChanged_ = function(managedUsers) {
    this.promise_ = this.createPromise_();
    this.resolve_(managedUsers);
    for (var i = 0; i < this.observers_.length; ++i)
      this.observers_[i].receiveExistingManagedUsers_(managedUsers);
  };

  /**
   * Adds an observer to the list of observers.
   * @param {Object} observer The observer to be added.
   * @private
   */
  ManagedUserListData.prototype.addObserver_ = function(observer) {
    for (var i = 0; i < this.observers_.length; ++i)
      assert(this.observers_[i] != observer);
    this.observers_.push(observer);
  };

  /**
   * Removes an observer from the list of observers.
   * @param {Object} observer The observer to be removed.
   * @private
   */
  ManagedUserListData.prototype.removeObserver_ = function(observer) {
    for (var i = 0; i < this.observers_.length; ++i) {
      if (this.observers_[i] == observer) {
        this.observers_.splice(i, 1);
        return;
      }
    }
  };

  // Forward public APIs to private implementations.
  [
    'addObserver',
    'onSigninError',
    'receiveExistingManagedUsers',
    'removeObserver',
    'requestExistingManagedUsers',
    'resetPromise',
  ].forEach(function(name) {
    ManagedUserListData[name] = function() {
      var instance = ManagedUserListData.getInstance();
      return instance[name + '_'].apply(instance, arguments);
    };
  });

  // Export
  return {
    ManagedUserListData: ManagedUserListData,
  };
});
