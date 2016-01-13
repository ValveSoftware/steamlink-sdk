// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Script to be injected into SAML provider pages, serving three main purposes:
 * 1. Signal hosting extension that an external page is loaded so that the
 *    UI around it should be changed accordingly;
 * 2. Provide an API via which the SAML provider can pass user credentials to
 *    Chrome OS, allowing the password to be used for encrypting user data and
 *    offline login.
 * 3. Scrape password fields, making the password available to Chrome OS even if
 *    the SAML provider does not support the credential passing API.
 */

(function() {
  function APICallForwarder() {
  }

  /**
   * The credential passing API is used by sending messages to the SAML page's
   * |window| object. This class forwards API calls from the SAML page to a
   * background script and API responses from the background script to the SAML
   * page. Communication with the background script occurs via a |Channel|.
   */
  APICallForwarder.prototype = {
    // Channel to which API calls are forwarded.
    channel_: null,

    /**
     * Initialize the API call forwarder.
     * @param {!Object} channel Channel to which API calls should be forwarded.
     */
    init: function(channel) {
      this.channel_ = channel;
      this.channel_.registerMessage('apiResponse',
                                    this.onAPIResponse_.bind(this));

      window.addEventListener('message', this.onMessage_.bind(this));
    },

    onMessage_: function(event) {
      if (event.source != window ||
          typeof event.data != 'object' ||
          !event.data.hasOwnProperty('type') ||
          event.data.type != 'gaia_saml_api') {
        return;
      }
      // Forward API calls to the background script.
      this.channel_.send({name: 'apiCall', call: event.data.call});
    },

    onAPIResponse_: function(msg) {
      // Forward API responses to the SAML page.
      window.postMessage({type: 'gaia_saml_api_reply', response: msg.response},
                         '/');
    }
  };

  /**
   * A class to scrape password from type=password input elements under a given
   * docRoot and send them back via a Channel.
   */
  function PasswordInputScraper() {
  }

  PasswordInputScraper.prototype = {
    // URL of the page.
    pageURL_: null,

    // Channel to send back changed password.
    channel_: null,

    // An array to hold password fields.
    passwordFields_: null,

    // An array to hold cached password values.
    passwordValues_: null,

    /**
     * Initialize the scraper with given channel and docRoot. Note that the
     * scanning for password fields happens inside the function and does not
     * handle DOM tree changes after the call returns.
     * @param {!Object} channel The channel to send back password.
     * @param {!string} pageURL URL of the page.
     * @param {!HTMLElement} docRoot The root element of the DOM tree that
     *     contains the password fields of interest.
     */
    init: function(channel, pageURL, docRoot) {
      this.pageURL_ = pageURL;
      this.channel_ = channel;

      this.passwordFields_ = docRoot.querySelectorAll('input[type=password]');
      this.passwordValues_ = [];

      for (var i = 0; i < this.passwordFields_.length; ++i) {
        this.passwordFields_[i].addEventListener(
            'input', this.onPasswordChanged_.bind(this, i));

        this.passwordValues_[i] = this.passwordFields_[i].value;
      }
    },

    /**
     * Check if the password field at |index| has changed. If so, sends back
     * the updated value.
     */
    maybeSendUpdatedPassword: function(index) {
      var newValue = this.passwordFields_[index].value;
      if (newValue == this.passwordValues_[index])
        return;

      this.passwordValues_[index] = newValue;

      // Use an invalid char for URL as delimiter to concatenate page url and
      // password field index to construct a unique ID for the password field.
      var passwordId = this.pageURL_ + '|' + index;
      this.channel_.send({
        name: 'updatePassword',
        id: passwordId,
        password: newValue
      });
    },

    /**
     * Handles 'change' event in the scraped password fields.
     * @param {number} index The index of the password fields in
     *     |passwordFields_|.
     */
    onPasswordChanged_: function(index) {
      this.maybeSendUpdatedPassword(index);
    }
  };

  /**
   * Returns true if the script is injected into auth main page.
   */
  function isAuthMainPage() {
    return window.location.href.indexOf(
        'chrome-extension://mfffpogegjflfpflabcdkioaeobkgjik/main.html') == 0;
  }

  /**
   * Heuristic test whether the current page is a relevant SAML page.
   * Current implementation checks if it is a http or https page and has
   * some content in it.
   * @return {boolean} Whether the current page looks like a SAML page.
   */
  function isSAMLPage() {
    var url = window.location.href;
    if (!url.match(/^(http|https):\/\//))
      return false;

    return document.body.scrollWidth > 50 && document.body.scrollHeight > 50;
  }

  if (isAuthMainPage()) {
    // Use an event to signal the auth main to enable SAML support.
    var e = document.createEvent('Event');
    e.initEvent('enableSAML', false, false);
    document.dispatchEvent(e);
  } else {
    var channel;
    var passwordScraper;
    if (isSAMLPage()) {
      var pageURL = window.location.href;

      channel = new Channel();
      channel.connect('injected');
      channel.send({name: 'pageLoaded', url: pageURL});

      apiCallForwarder = new APICallForwarder();
      apiCallForwarder.init(channel);

      passwordScraper = new PasswordInputScraper();
      passwordScraper.init(channel, pageURL, document.documentElement);
    }
  }
})();
